/*
 * Copyright (C) 2013  Jan Dvorak <mordae@anilinux.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "cctalk.h"

#include <stdlib.h>

inline static int detect_support(const struct cctalk_device *dev,
                                 enum cctalk_method method)
{
	if (-1 == cctalk_send(dev->host, dev->id, method, NULL, 0))
		return 0;

	if (-1 == cctalk_recv_status(dev->host))
		return 0;

	return 1;
}


struct cctalk_device *cctalk_device_scan(const struct cctalk_host *host,
                                         uint8_t id)
{
	uint8_t vers[3] = {0, 0, 0};

	if (-1 == cctalk_send(host, id, 4, NULL, 0))
		return NULL;

	if (0 != cctalk_recv_data(host, vers, sizeof(vers)))
		return NULL;

	/* Create the basic device description. */
	struct cctalk_device *dev = calloc(1, sizeof(*dev));
	dev->host = host;
	dev->id = id;
	dev->version = (vers[1] << 8) | vers[2];
	dev->coin_mask = 0xffff;

	dev->has_master_inhibit_status = detect_support(dev, 227) &&
	                                 detect_support(dev, 228);

	dev->has_inhibit_status = detect_support(dev, 230) &&
	                          detect_support(dev, 231);

	return dev;
}

void cctalk_device_free(struct cctalk_device *dev)
{
	if (NULL == dev)
		return;

	free(dev);
}

static int set_master_inhibit_status(const struct cctalk_device *dev, int on)
{
	uint8_t data[1] = {on ? 1 : 0};

	if (-1 == cctalk_send(dev->host, dev->id, 228, data, 1))
		return -1;

	return 0 == cctalk_recv_status(dev->host);
}

static int set_inhibit_status(const struct cctalk_device *dev, uint16_t mask)
{
	uint8_t data[2] = {mask & 0xff, mask >> 8};

	if (-1 == cctalk_send(dev->host, dev->id, 231, data, 2))
		return -1;

	return 0 == cctalk_recv_status(dev->host);
}

int cctalk_device_set_accept_coins(const struct cctalk_device *dev, int on)
{
	if (dev->has_master_inhibit_status)
		return set_master_inhibit_status(dev, on);

	if (dev->has_inhibit_status)
		return set_inhibit_status(dev, on ? dev->coin_mask : 0x0000);

	return 0;
}

int cctalk_device_set_coin_mask(struct cctalk_device *dev, uint16_t mask)
{
	dev->coin_mask = mask;

	if (dev->has_inhibit_status)
		return set_inhibit_status(dev, mask);

	return 0;
}

int cctalk_device_query_credits(const struct cctalk_device *dev,
                                struct cctalk_credit_info *info)
{
	uint8_t result[11] = {0};
	size_t i;

	if (-1 == cctalk_send(dev->host, dev->id, 229, NULL, 0))
		return -1;

	if (0 != cctalk_recv_data(dev->host, result, sizeof(result)))
		return -1;

	info->seq = result[0];

	for (i = 0; i < 5; i++) {
		info->coins[i].value  = result[1 + 2 * i];
		info->coins[i].sorter = result[2 + 2 * i];
		info->coins[i].error  = result[2 + 2 * i];
	}

	return 0;
}

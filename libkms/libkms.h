/**************************************************************************
 *
 * Copyright © 2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/


#ifndef _LIBKMS_H_
#define _LIBKMS_H_

struct kms_driver;
struct kms_bo;

enum kms_attrib
{
	KMS_TERMINATE_PROP_LIST,
	KMS_BO_TYPE,
	KMS_WIDTH,
	KMS_HEIGHT,
	KMS_PITCH,
	KMS_HANDLE,
	KMS_MAX_SCANOUT_WIDTH,
	KMS_MAX_SCANOUT_HEIGHT,
	KMS_MIN_SCANOUT_WIDTH,
	KMS_MIN_SCANOUT_HEIGHT,
	KMS_MAX_CURSOR_WIDTH,
	KMS_MAX_CURSOR_HEIGHT,
	KMS_MIN_CURSOR_WIDTH,
	KMS_MIN_CURSOR_HEIGHT,
};

enum kms_bo_type
{
	KMS_BO_TYPE_SCANOUT = (1 << 0),
	KMS_BO_TYPE_CURSOR =  (1 << 1),
};

int kms_create(int fd, struct kms_driver **out);
int kms_get_prop(struct kms_driver *kms, unsigned key, unsigned *out);
int kms_destroy(struct kms_driver **kms);

int kms_bo_create(struct kms_driver *kms, const unsigned *attr, struct kms_bo **out);
int kms_bo_get_prop(struct kms_bo *bo, unsigned key, unsigned *out);
int kms_bo_map(struct kms_bo *bo, void **out);
int kms_bo_unmap(struct kms_bo *bo);
int kms_bo_destroy(struct kms_bo **bo);

#endif

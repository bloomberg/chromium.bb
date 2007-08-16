/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * XGI AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ***************************************************************************/

#ifndef _XGI_CMDLIST_H_
#define _XGI_CMDLIST_H_

struct xgi_cmdring_info {
	/**
	 * Kernel space pointer to the base of the command ring.
	 */
	u32 * ptr;

	/**
	 * Size, in bytes, of the command ring.
	 */
	unsigned int size;

	/**
	 * Base address of the command ring from the hardware's PoV.
	 */
	unsigned int ring_hw_base;

	u32 * last_ptr;

	/**
	 * Offset, in bytes, from the start of the ring to the next available
	 * location to store a command.
	 */
	unsigned int ring_offset;
};

struct xgi_info;
extern int xgi_cmdlist_initialize(struct xgi_info * info, size_t size,
	struct drm_file * filp);

extern int xgi_state_change(struct xgi_info * info, unsigned int to,
	unsigned int from);

extern void xgi_cmdlist_cleanup(struct xgi_info * info);

extern void xgi_emit_irq(struct xgi_info * info);

#endif				/* _XGI_CMDLIST_H_ */

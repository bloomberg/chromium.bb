/* compat-pre24.h -- Linux pre-2.4.0 compatibility
 * Created: Mon Aug 28 15:31:43 2000 by faith@acm.org
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 * 
 */

#ifndef _COMPAT_PRE24_H_
#define _COMPAT_PRE24_H_

				/* virt_to_page added in 2.4.0-test6 */
#if LINUX_VERSION_CODE < 0x020400
#define virt_to_page(kaddr) (mem_map + MAP_NR(kaddr))
#endif

#define block_all_signals(a,b,c)
#define unblock_all_signals()

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
#define __exit
#endif

#endif

/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __SAVAGE_H__
#define __SAVAGE_H__

/* This remains constant for all DRM template files.
 */
#define DRM(x) savage_##x

/* General customization:
 */
#define __HAVE_AGP		1
#define __MUST_HAVE_AGP		1
#define __HAVE_MTRR		1
#define __HAVE_CTX_BITMAP	1

#define DRIVER_PCI_IDS							\
        {0x5333, 0x8a22, 0, "Savage4"},                                 \
        {0x5333, 0x8a23, 0, "Savage4"},                                 \
	{0x5333, 0x8c10, 0, "Savage/MX-MV"},				\
	{0x5333, 0x8c11, 0, "Savage/MX"},				\
	{0x5333, 0x8c12, 0, "Savage/IX-MV"},				\
	{0x5333, 0x8c13, 0, "Savage/IX"},				\
	{0x5333, 0x8c20, 0, "Savage 3D"},				\
	{0x5333, 0x8c21, 0, "Savage 3D/MV"},				\
	{0x5333, 0x8c22, 0, "SuperSavage MX/128"},			\
	{0x5333, 0x8c24, 0, "SuperSavage MX/64"},			\
	{0x5333, 0x8c26, 0, "SuperSavage MX/64C"},			\
	{0x5333, 0x8c2a, 0, "SuperSavage IX/128 SDR"},			\
	{0x5333, 0x8c2b, 0, "SuperSavage IX/128 DDR"},			\
	{0x5333, 0x8c2c, 0, "SuperSavage IX/64 SDR"},			\
	{0x5333, 0x8c2d, 0, "SuperSavage IX/64 DDR"},			\
	{0x5333, 0x8c2e, 0, "SuperSavage IX/C SDR"},			\
	{0x5333, 0x8c2f, 0, "SuperSavage IX/C DDR"},			\
	{0x5333, 0x8a25, 0, "ProSavage PM133"},				\
	{0x5333, 0x8a26, 0, "ProSavage KM133"},				\
	{0x5333, 0x8d01, 0, "ProSavage PN133"},				\
	{0x5333, 0x8d02, 0, "ProSavage KN133"},				\
	{0x5333, 0x8d04, 0, "ProSavage DDR"}

#endif

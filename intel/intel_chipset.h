/*
 *
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _INTEL_CHIPSET_H
#define _INTEL_CHIPSET_H

#define IS_830(dev) ((dev)->pci_device == 0x3577)
#define IS_845(dev) ((dev)->pci_device == 0x2562)
#define IS_85X(dev) ((dev)->pci_device == 0x3582)
#define IS_865(dev) ((dev)->pci_device == 0x2572)

#define IS_GEN2(dev) (IS_830(dev) ||				\
		      IS_845(dev) ||				\
		      IS_85X(dev) ||				\
		      IS_865(dev))

#define IS_915G(dev) ((dev)->pci_device == 0x2582 ||		\
		       (dev)->pci_device == 0x258a)
#define IS_915GM(dev) ((dev)->pci_device == 0x2592)
#define IS_945G(dev) ((dev)->pci_device == 0x2772)
#define IS_945GM(dev) ((dev)->pci_device == 0x27A2 ||		\
                        (dev)->pci_device == 0x27AE)

#define IS_915(dev) (IS_915G(dev) ||				\
		     IS_915GM(dev))

#define IS_945(dev) (IS_945G(dev) ||				\
		     IS_945GM(dev) ||				\
		     IS_G33(dev) ||				\
		     IS_PINEVIEW(dev))

#define IS_G33(dev)    ((dev)->pci_device == 0x29C2 ||		\
                        (dev)->pci_device == 0x29B2 ||		\
                        (dev)->pci_device == 0x29D2)

#define IS_PINEVIEW(dev) ((dev)->pci_device == 0xa001 ||	\
			  (dev)->pci_device == 0xa011)

#define IS_GEN3(dev) (IS_915(dev) ||				\
		      IS_945(dev) ||				\
		      IS_G33(dev) ||				\
		      IS_PINEVIEW(dev))

#define IS_I965GM(dev) ((dev)->pci_device == 0x2A02)

#define IS_GEN4(dev) ((dev)->pci_device == 0x2972 ||	\
		      (dev)->pci_device == 0x2982 ||	\
		      (dev)->pci_device == 0x2992 ||	\
		      (dev)->pci_device == 0x29A2 ||	\
		      (dev)->pci_device == 0x2A02 ||	\
		      (dev)->pci_device == 0x2A12 ||	\
		      (dev)->pci_device == 0x2A42 ||	\
		      (dev)->pci_device == 0x2E02 ||	\
		      (dev)->pci_device == 0x2E12 ||	\
		      (dev)->pci_device == 0x2E22 ||	\
		      (dev)->pci_device == 0x2E32 ||	\
		      (dev)->pci_device == 0x2E42 ||	\
		      (dev)->pci_device == 0x0042 ||	\
		      (dev)->pci_device == 0x0046 ||	\
		      IS_I965GM(dev) || \
		      IS_G4X(dev))

#define IS_GM45(dev) ((dev)->pci_device == 0x2A42)

#define IS_G4X(dev) ((dev)->pci_device == 0x2E02 || \
                     (dev)->pci_device == 0x2E12 || \
                     (dev)->pci_device == 0x2E22 || \
                     (dev)->pci_device == 0x2E32 || \
                     (dev)->pci_device == 0x2E42 || \
		     IS_GM45(dev))

#define IS_9XX(dev) (IS_GEN3(dev) ||				\
		     IS_GEN4(dev) ||				\
		     IS_GEN5(dev) ||				\
		     IS_GEN6(dev))

#endif /* _INTEL_CHIPSET_H */

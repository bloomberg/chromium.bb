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

#include "xgi_drv.h"
#include "xgi_regs.h"

#include <linux/delay.h>

/*
 * irq functions
 */
#define STALL_INTERRUPT_RESET_THRESHOLD 0xffff

static unsigned int s_invalid_begin = 0;

static bool xgi_validate_signal(struct drm_map * map)
{
	if (le32_to_cpu(DRM_READ32(map, 0x2800) & 0x001c0000)) {
		u16 check;

		/* Check Read back status */
		DRM_WRITE8(map, 0x235c, 0x80);
		check = le16_to_cpu(DRM_READ16(map, 0x2360));

		if ((check & 0x3f) != ((check & 0x3f00) >> 8)) {
			return FALSE;
		}

		/* Check RO channel */
		DRM_WRITE8(map, 0x235c, 0x83);
		check = le16_to_cpu(DRM_READ16(map, 0x2360));
		if ((check & 0x0f) != ((check & 0xf0) >> 4)) {
			return FALSE;
		}

		/* Check RW channel */
		DRM_WRITE8(map, 0x235c, 0x88);
		check = le16_to_cpu(DRM_READ16(map, 0x2360));
		if ((check & 0x0f) != ((check & 0xf0) >> 4)) {
			return FALSE;
		}

		/* Check RO channel outstanding */
		DRM_WRITE8(map, 0x235c, 0x8f);
		check = le16_to_cpu(DRM_READ16(map, 0x2360));
		if (0 != (check & 0x3ff)) {
			return FALSE;
		}

		/* Check RW channel outstanding */
		DRM_WRITE8(map, 0x235c, 0x90);
		check = le16_to_cpu(DRM_READ16(map, 0x2360));
		if (0 != (check & 0x3ff)) {
			return FALSE;
		}

		/* No pending PCIE request. GE stall. */
	}

	return TRUE;
}


static void xgi_ge_hang_reset(struct drm_map * map)
{
	int time_out = 0xffff;

	DRM_WRITE8(map, 0xb057, 8);
	while (0 != le32_to_cpu(DRM_READ32(map, 0x2800) & 0xf0000000)) {
		while (0 != ((--time_out) & 0xfff))
			/* empty */ ;

		if (0 == time_out) {
			u8 old_3ce;
			u8 old_3cf;
			u8 old_index;
			u8 old_36;

			DRM_INFO("Can not reset back 0x%x!\n",
				 le32_to_cpu(DRM_READ32(map, 0x2800)));

			DRM_WRITE8(map, 0xb057, 0);

			/* Have to use 3x5.36 to reset. */
			/* Save and close dynamic gating */

			old_3ce = DRM_READ8(map, 0x3ce);
			DRM_WRITE8(map, 0x3ce, 0x2a);
			old_3cf = DRM_READ8(map, 0x3cf);
			DRM_WRITE8(map, 0x3cf, old_3cf & 0xfe);

			/* Reset GE */
			old_index = DRM_READ8(map, 0x3d4);
			DRM_WRITE8(map, 0x3d4, 0x36);
			old_36 = DRM_READ8(map, 0x3d5);
			DRM_WRITE8(map, 0x3d5, old_36 | 0x10);

			while (0 != ((--time_out) & 0xfff))
				/* empty */ ;

			DRM_WRITE8(map, 0x3d5, old_36);
			DRM_WRITE8(map, 0x3d4, old_index);

			/* Restore dynamic gating */
			DRM_WRITE8(map, 0x3cf, old_3cf);
			DRM_WRITE8(map, 0x3ce, old_3ce);
			break;
		}
	}

	DRM_WRITE8(map, 0xb057, 0);
}


bool xgi_ge_irq_handler(struct xgi_info * info)
{
	const u32 int_status = le32_to_cpu(DRM_READ32(info->mmio_map, 0x2810));
	bool is_support_auto_reset = FALSE;

	/* Check GE on/off */
	if (0 == (0xffffc0f0 & int_status)) {
		if (0 != (0x1000 & int_status)) {
			/* We got GE stall interrupt.
			 */
			DRM_WRITE32(info->mmio_map, 0x2810,
				    cpu_to_le32(int_status | 0x04000000));

			if (is_support_auto_reset) {
				static cycles_t last_tick;
				static unsigned continue_int_count = 0;

				/* OE II is busy. */

				if (!xgi_validate_signal(info->mmio_map)) {
					/* Nothing but skip. */
				} else if (0 == continue_int_count++) {
					last_tick = get_cycles();
				} else {
					const cycles_t new_tick = get_cycles();
					if ((new_tick - last_tick) >
					    STALL_INTERRUPT_RESET_THRESHOLD) {
						continue_int_count = 0;
					} else if (continue_int_count >= 3) {
						continue_int_count = 0;

						/* GE Hung up, need reset. */
						DRM_INFO("Reset GE!\n");

						xgi_ge_hang_reset(info->mmio_map);
					}
				}
			}
		} else if (0 != (0x1 & int_status)) {
			s_invalid_begin++;
			DRM_WRITE32(info->mmio_map, 0x2810,
				    cpu_to_le32((int_status & ~0x01) | 0x04000000));
		}

		return TRUE;
	}

	return FALSE;
}

bool xgi_crt_irq_handler(struct xgi_info * info)
{
	bool ret = FALSE;
	u8 save_3ce = DRM_READ8(info->mmio_map, 0x3ce);

	/* CRT1 interrupt just happened
	 */
	if (IN3CFB(info->mmio_map, 0x37) & 0x01) {
		u8 op3cf_3d;
		u8 op3cf_37;

		/* What happened?
		 */
		op3cf_37 = IN3CFB(info->mmio_map, 0x37);

		/* Clear CRT interrupt
		 */
		op3cf_3d = IN3CFB(info->mmio_map, 0x3d);
		OUT3CFB(info->mmio_map, 0x3d, (op3cf_3d | 0x04));
		OUT3CFB(info->mmio_map, 0x3d, (op3cf_3d & ~0x04));
		ret = TRUE;
	}
	DRM_WRITE8(info->mmio_map, 0x3ce, save_3ce);

	return (ret);
}

bool xgi_dvi_irq_handler(struct xgi_info * info)
{
	bool ret = FALSE;
	const u8 save_3ce = DRM_READ8(info->mmio_map, 0x3ce);

	/* DVI interrupt just happened
	 */
	if (IN3CFB(info->mmio_map, 0x38) & 0x20) {
		const u8 save_3x4 = DRM_READ8(info->mmio_map, 0x3d4);
		u8 op3cf_39;
		u8 op3cf_37;
		u8 op3x5_5a;

		/* What happened?
		 */
		op3cf_37 = IN3CFB(info->mmio_map, 0x37);

		/* Notify BIOS that DVI plug/unplug happened
		 */
		op3x5_5a = IN3X5B(info->mmio_map, 0x5a);
		OUT3X5B(info->mmio_map, 0x5a, op3x5_5a & 0xf7);

		DRM_WRITE8(info->mmio_map, 0x3d4, save_3x4);

		/* Clear DVI interrupt
		 */
		op3cf_39 = IN3CFB(info->mmio_map, 0x39);
		OUT3C5B(info->mmio_map, 0x39, (op3cf_39 & ~0x01));
		OUT3C5B(info->mmio_map, 0x39, (op3cf_39 | 0x01));

		ret = TRUE;
	}
	DRM_WRITE8(info->mmio_map, 0x3ce, save_3ce);

	return (ret);
}


static void dump_reg_header(unsigned regbase)
{
	printk("\n=====xgi_dump_register========0x%x===============\n",
	       regbase);
	printk("    0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
}


static void dump_indexed_reg(struct xgi_info * info, unsigned regbase)
{
	unsigned i, j;
	u8 temp;


	dump_reg_header(regbase);
	for (i = 0; i < 0x10; i++) {
		printk("%1x ", i);

		for (j = 0; j < 0x10; j++) {
			DRM_WRITE8(info->mmio_map, regbase - 1,
				   (i * 0x10) + j);
			temp = DRM_READ8(info->mmio_map, regbase);
			printk("%3x", temp);
		}
		printk("\n");
	}
}


static void dump_reg(struct xgi_info * info, unsigned regbase, unsigned range)
{
	unsigned i, j;


	dump_reg_header(regbase);
	for (i = 0; i < range; i++) {
		printk("%1x ", i);

		for (j = 0; j < 0x10; j++) {
			u8 temp = DRM_READ8(info->mmio_map,
					    regbase + (i * 0x10) + j);
			printk("%3x", temp);
		}
		printk("\n");
	}
}


void xgi_dump_register(struct xgi_info * info)
{
	dump_indexed_reg(info, 0x3c5);
	dump_indexed_reg(info, 0x3d5);
	dump_indexed_reg(info, 0x3cf);

	dump_reg(info, 0xB000, 0x05);
	dump_reg(info, 0x2200, 0x0B);
	dump_reg(info, 0x2300, 0x07);
	dump_reg(info, 0x2400, 0x10);
	dump_reg(info, 0x2800, 0x10);
}


#define WHOLD_GE_STATUS             0x2800

/* Test everything except the "whole GE busy" bit, the "master engine busy"
 * bit, and the reserved bits [26:21].
 */
#define IDLE_MASK                   ~((1U<<31) | (1U<<28) | (0x3f<<21))

void xgi_waitfor_pci_idle(struct xgi_info * info)
{
	unsigned int idleCount = 0;
	u32 old_status = 0;
	unsigned int same_count = 0;

	while (idleCount < 5) {
		const u32 status = DRM_READ32(info->mmio_map, WHOLD_GE_STATUS)
			& IDLE_MASK;

		if (status == old_status) {
			same_count++;

			if ((same_count % 100) == 0) {
				DRM_ERROR("GE status stuck at 0x%08x for %u iterations!\n",
					  old_status, same_count);
			}
		} else {
			old_status = status;
			same_count = 0;
		}

		if (status != 0) {
			msleep(1);
			idleCount = 0;
		} else {
			idleCount++;
		}
	}
}


void xgi_enable_mmio(struct xgi_info * info)
{
	u8 protect = 0;
	u8 temp;

	/* Unprotect registers */
	DRM_WRITE8(info->mmio_map, 0x3C4, 0x11);
	protect = DRM_READ8(info->mmio_map, 0x3C5);
	DRM_WRITE8(info->mmio_map, 0x3C5, 0x92);

	DRM_WRITE8(info->mmio_map, 0x3D4, 0x3A);
	temp = DRM_READ8(info->mmio_map, 0x3D5);
	DRM_WRITE8(info->mmio_map, 0x3D5, temp | 0x20);

	/* Enable MMIO */
	DRM_WRITE8(info->mmio_map, 0x3D4, 0x39);
	temp = DRM_READ8(info->mmio_map, 0x3D5);
	DRM_WRITE8(info->mmio_map, 0x3D5, temp | 0x01);

	/* Protect registers */
	OUT3C5B(info->mmio_map, 0x11, protect);
}


void xgi_disable_mmio(struct xgi_info * info)
{
	u8 protect = 0;
	u8 temp;

	/* Unprotect registers */
	DRM_WRITE8(info->mmio_map, 0x3C4, 0x11);
	protect = DRM_READ8(info->mmio_map, 0x3C5);
	DRM_WRITE8(info->mmio_map, 0x3C5, 0x92);

	/* Disable MMIO access */
	DRM_WRITE8(info->mmio_map, 0x3D4, 0x39);
	temp = DRM_READ8(info->mmio_map, 0x3D5);
	DRM_WRITE8(info->mmio_map, 0x3D5, temp & 0xFE);

	/* Protect registers */
	OUT3C5B(info->mmio_map, 0x11, protect);
}


void xgi_enable_ge(struct xgi_info * info)
{
	u8 bOld3cf2a;
	int wait = 0;

	OUT3C5B(info->mmio_map, 0x11, 0x92);

	/* Save and close dynamic gating
	 */
	bOld3cf2a = IN3CFB(info->mmio_map, XGI_MISC_CTRL);
	OUT3CFB(info->mmio_map, XGI_MISC_CTRL, bOld3cf2a & ~EN_GEPWM);

	/* Enable 2D and 3D GE
	 */
	OUT3X5B(info->mmio_map, XGI_GE_CNTL, (GE_ENABLE | GE_ENABLE_3D));
	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}

	/* Reset both 3D and 2D engine
	 */
	OUT3X5B(info->mmio_map, XGI_GE_CNTL,
		(GE_ENABLE | GE_RESET | GE_ENABLE_3D));
	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}

	OUT3X5B(info->mmio_map, XGI_GE_CNTL, (GE_ENABLE | GE_ENABLE_3D));
	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}

	/* Enable 2D engine only
	 */
	OUT3X5B(info->mmio_map, XGI_GE_CNTL, GE_ENABLE);

	/* Enable 2D+3D engine
	 */
	OUT3X5B(info->mmio_map, XGI_GE_CNTL, (GE_ENABLE | GE_ENABLE_3D));

	/* Restore dynamic gating
	 */
	OUT3CFB(info->mmio_map, XGI_MISC_CTRL, bOld3cf2a);
}


void xgi_disable_ge(struct xgi_info * info)
{
	int wait = 0;

	OUT3X5B(info->mmio_map, XGI_GE_CNTL, (GE_ENABLE | GE_ENABLE_3D));

	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}

	/* Reset both 3D and 2D engine
	 */
	OUT3X5B(info->mmio_map, XGI_GE_CNTL,
		(GE_ENABLE | GE_RESET | GE_ENABLE_3D));

	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}
	OUT3X5B(info->mmio_map, XGI_GE_CNTL, (GE_ENABLE | GE_ENABLE_3D));

	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}

	/* Disable 2D engine and 3D engine.
	 */
	OUT3X5B(info->mmio_map, XGI_GE_CNTL, 0);
}

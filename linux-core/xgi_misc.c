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

int xgi_ge_reset_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	struct xgi_info *info = dev->dev_private;

	xgi_disable_ge(info);
	xgi_enable_ge(info);

	return 0;
}


/*
 * irq functions
 */
#define STALL_INTERRUPT_RESET_THRESHOLD 0xffff

static unsigned int s_invalid_begin = 0;

static bool xgi_validate_signal(volatile u8 *mmio_vbase)
{
	volatile u32 *const ge_3d_status = 
		(volatile u32 *)(mmio_vbase + 0x2800);
	const u32 old_ge_status = ge_3d_status[0x00];

	if (old_ge_status & 0x001c0000) {
		u16 check;

		/* Check Read back status */
		*(mmio_vbase + 0x235c) = 0x80;
		check = *((volatile u16 *)(mmio_vbase + 0x2360));

		if ((check & 0x3f) != ((check & 0x3f00) >> 8)) {
			return FALSE;
		}

		/* Check RO channel */
		*(mmio_vbase + 0x235c) = 0x83;
		check = *((volatile u16 *)(mmio_vbase + 0x2360));
		if ((check & 0x0f) != ((check & 0xf0) >> 4)) {
			return FALSE;
		}

		/* Check RW channel */
		*(mmio_vbase + 0x235c) = 0x88;
		check = *((volatile u16 *)(mmio_vbase + 0x2360));
		if ((check & 0x0f) != ((check & 0xf0) >> 4)) {
			return FALSE;
		}

		/* Check RO channel outstanding */
		*(mmio_vbase + 0x235c) = 0x8f;
		check = *((volatile u16 *)(mmio_vbase + 0x2360));
		if (0 != (check & 0x3ff)) {
			return FALSE;
		}

		/* Check RW channel outstanding */
		*(mmio_vbase + 0x235c) = 0x90;
		check = *((volatile u16 *)(mmio_vbase + 0x2360));
		if (0 != (check & 0x3ff)) {
			return FALSE;
		}

		/* No pending PCIE request. GE stall. */
	}

	return TRUE;
}


static void xgi_ge_hang_reset(volatile u8 *mmio_vbase)
{
	volatile u32 *const ge_3d_status =
		(volatile u32 *)(mmio_vbase + 0x2800);
	int time_out = 0xffff;

	*(mmio_vbase + 0xb057) = 8;
	while (0 != (ge_3d_status[0x00] & 0xf0000000)) {
		while (0 != ((--time_out) & 0xfff)) 
			/* empty */ ;

		if (0 == time_out) {
			u8 old_3ce;
			u8 old_3cf;
			u8 old_index;
			u8 old_36;

			DRM_INFO("Can not reset back 0x%x!\n",
				 ge_3d_status[0x00]);

			*(mmio_vbase + 0xb057) = 0;

			/* Have to use 3x5.36 to reset. */
			/* Save and close dynamic gating */

			old_3ce = *(mmio_vbase + 0x3ce);
			*(mmio_vbase + 0x3ce) = 0x2a;
			old_3cf = *(mmio_vbase + 0x3cf);
			*(mmio_vbase + 0x3cf) = old_3cf & 0xfe;

			/* Reset GE */
			old_index = *(mmio_vbase + 0x3d4);
			*(mmio_vbase + 0x3d4) = 0x36;
			old_36 = *(mmio_vbase + 0x3d5);
			*(mmio_vbase + 0x3d5) = old_36 | 0x10;

			while (0 != ((--time_out) & 0xfff)) 
				/* empty */ ;

			*(mmio_vbase + 0x3d5) = old_36;
			*(mmio_vbase + 0x3d4) = old_index;

			/* Restore dynamic gating */
			*(mmio_vbase + 0x3cf) = old_3cf; 
			*(mmio_vbase + 0x3ce) = old_3ce;
			break;
		}
	}

	*(mmio_vbase + 0xb057) = 0;
}

	
bool xgi_ge_irq_handler(struct xgi_info * info)
{
	volatile u8 *const mmio_vbase = info->mmio_map->handle;
	volatile u32 *const ge_3d_status =
		(volatile u32 *)(mmio_vbase + 0x2800);
	const u32 int_status = ge_3d_status[4];
	bool is_support_auto_reset = FALSE;

	/* Check GE on/off */
	if (0 == (0xffffc0f0 & int_status)) {
		u32 old_pcie_cmd_fetch_Addr = ge_3d_status[0x0a];

		if (0 != (0x1000 & int_status)) {
			/* We got GE stall interrupt. 
			 */
			ge_3d_status[0x04] = int_status | 0x04000000;

			if (is_support_auto_reset) {
				static cycles_t last_tick;
				static unsigned continue_int_count = 0;

				/* OE II is busy. */

				if (!xgi_validate_signal(mmio_vbase)) {
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

						xgi_ge_hang_reset(mmio_vbase);
					}
				}
			}
		} else if (0 != (0x1 & int_status)) {
			s_invalid_begin++;
			ge_3d_status[0x04] = (int_status & ~0x01) | 0x04000000;
		}

		return TRUE;
	}

	return FALSE;
}

bool xgi_crt_irq_handler(struct xgi_info * info)
{
	bool ret = FALSE;
	u8 save_3ce = DRM_READ8(info->mmio_map, 0x3ce);

	if (IN3CFB(info->mmio_map, 0x37) & 0x01)	// CRT1 interrupt just happened
	{
		u8 op3cf_3d;
		u8 op3cf_37;

		// What happened?
		op3cf_37 = IN3CFB(info->mmio_map, 0x37);

		// Clear CRT interrupt
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

	if (IN3CFB(info->mmio_map, 0x38) & 0x20) {	// DVI interrupt just happened
		const u8 save_3x4 = DRM_READ8(info->mmio_map, 0x3d4);
		u8 op3cf_39;
		u8 op3cf_37;
		u8 op3x5_5a;

		// What happened?
		op3cf_37 = IN3CFB(info->mmio_map, 0x37);

		//Notify BIOS that DVI plug/unplug happened
		op3x5_5a = IN3X5B(info->mmio_map, 0x5a);
		OUT3X5B(info->mmio_map, 0x5a, op3x5_5a & 0xf7);

		DRM_WRITE8(info->mmio_map, 0x3d4, save_3x4);

		// Clear DVI interrupt
		op3cf_39 = IN3CFB(info->mmio_map, 0x39);
		OUT3C5B(info->mmio_map, 0x39, (op3cf_39 & ~0x01));	//Set 3cf.39 bit 0 to 0
		OUT3C5B(info->mmio_map, 0x39, (op3cf_39 | 0x01));	//Set 3cf.39 bit 0 to 1

		ret = TRUE;
	}
	DRM_WRITE8(info->mmio_map, 0x3ce, save_3ce);

	return (ret);
}


void xgi_dump_register(struct xgi_info * info)
{
	int i, j;
	unsigned char temp;

	// 0x3C5
	printk("\r\n=====xgi_dump_register========0x%x===============\r\n",
	       0x3C5);

	for (i = 0; i < 0x10; i++) {
		if (i == 0) {
			printk("%5x", i);
		} else {
			printk("%3x", i);
		}
	}
	printk("\r\n");

	for (i = 0; i < 0x10; i++) {
		printk("%1x ", i);

		for (j = 0; j < 0x10; j++) {
			temp = IN3C5B(info->mmio_map, i * 0x10 + j);
			printk("%3x", temp);
		}
		printk("\r\n");
	}

	// 0x3D5
	printk("\r\n====xgi_dump_register=========0x%x===============\r\n",
	       0x3D5);
	for (i = 0; i < 0x10; i++) {
		if (i == 0) {
			printk("%5x", i);
		} else {
			printk("%3x", i);
		}
	}
	printk("\r\n");

	for (i = 0; i < 0x10; i++) {
		printk("%1x ", i);

		for (j = 0; j < 0x10; j++) {
			temp = IN3X5B(info->mmio_map, i * 0x10 + j);
			printk("%3x", temp);
		}
		printk("\r\n");
	}

	// 0x3CF
	printk("\r\n=========xgi_dump_register====0x%x===============\r\n",
	       0x3CF);
	for (i = 0; i < 0x10; i++) {
		if (i == 0) {
			printk("%5x", i);
		} else {
			printk("%3x", i);
		}
	}
	printk("\r\n");

	for (i = 0; i < 0x10; i++) {
		printk("%1x ", i);

		for (j = 0; j < 0x10; j++) {
			temp = IN3CFB(info->mmio_map, i * 0x10 + j);
			printk("%3x", temp);
		}
		printk("\r\n");
	}

	printk("\r\n=====xgi_dump_register======0x%x===============\r\n",
	       0xB000);
	for (i = 0; i < 0x10; i++) {
		if (i == 0) {
			printk("%5x", i);
		} else {
			printk("%3x", i);
		}
	}
	printk("\r\n");

	for (i = 0; i < 0x5; i++) {
		printk("%1x ", i);

		for (j = 0; j < 0x10; j++) {
			temp = DRM_READ8(info->mmio_map, 0xB000 + i * 0x10 + j);
			printk("%3x", temp);
		}
		printk("\r\n");
	}

	printk("\r\n==================0x%x===============\r\n", 0x2200);
	for (i = 0; i < 0x10; i++) {
		if (i == 0) {
			printk("%5x", i);
		} else {
			printk("%3x", i);
		}
	}
	printk("\r\n");

	for (i = 0; i < 0xB; i++) {
		printk("%1x ", i);

		for (j = 0; j < 0x10; j++) {
			temp = DRM_READ8(info->mmio_map, 0x2200 + i * 0x10 + j);
			printk("%3x", temp);
		}
		printk("\r\n");
	}

	printk("\r\n==================0x%x===============\r\n", 0x2300);
	for (i = 0; i < 0x10; i++) {
		if (i == 0) {
			printk("%5x", i);
		} else {
			printk("%3x", i);
		}
	}
	printk("\r\n");

	for (i = 0; i < 0x7; i++) {
		printk("%1x ", i);

		for (j = 0; j < 0x10; j++) {
			temp = DRM_READ8(info->mmio_map, 0x2300 + i * 0x10 + j);
			printk("%3x", temp);
		}
		printk("\r\n");
	}

	printk("\r\n==================0x%x===============\r\n", 0x2400);
	for (i = 0; i < 0x10; i++) {
		if (i == 0) {
			printk("%5x", i);
		} else {
			printk("%3x", i);
		}
	}
	printk("\r\n");

	for (i = 0; i < 0x10; i++) {
		printk("%1x ", i);

		for (j = 0; j < 0x10; j++) {
			temp = DRM_READ8(info->mmio_map, 0x2400 + i * 0x10 + j);
			printk("%3x", temp);
		}
		printk("\r\n");
	}

	printk("\r\n==================0x%x===============\r\n", 0x2800);
	for (i = 0; i < 0x10; i++) {
		if (i == 0) {
			printk("%5x", i);
		} else {
			printk("%3x", i);
		}
	}
	printk("\r\n");

	for (i = 0; i < 0x10; i++) {
		printk("%1x ", i);

		for (j = 0; j < 0x10; j++) {
			temp = DRM_READ8(info->mmio_map, 0x2800 + i * 0x10 + j);
			printk("%3x", temp);
		}
		printk("\r\n");
	}
}


int xgi_dump_register_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	struct xgi_info *info = dev->dev_private;

	xgi_dump_register(info);

	return 0;
}


int xgi_restore_registers_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	struct xgi_info *info = dev->dev_private;

	OUT3X5B(info->mmio_map, 0x13, 0);
	OUT3X5B(info->mmio_map, 0x8b, 2);

	return 0;
}

void xgi_waitfor_pci_idle(struct xgi_info * info)
{
#define WHOLD_GE_STATUS             0x2800
#define IDLE_MASK                   ~0x90200000

	int idleCount = 0;
	while (idleCount < 5) {
		if (DRM_READ32(info->mmio_map, WHOLD_GE_STATUS) & IDLE_MASK) {
			idleCount = 0;
		} else {
			idleCount++;
		}
	}
}

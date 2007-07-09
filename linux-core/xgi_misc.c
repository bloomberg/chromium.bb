
/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.			
 *																			*
 * All Rights Reserved.														*
 *																			*
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the	
 * "Software"), to deal in the Software without restriction, including	
 * without limitation on the rights to use, copy, modify, merge,	
 * publish, distribute, sublicense, and/or sell copies of the Software,	
 * and to permit persons to whom the Software is furnished to do so,	
 * subject to the following conditions:					
 *																			*
 * The above copyright notice and this permission notice (including the	
 * next paragraph) shall be included in all copies or substantial	
 * portions of the Software.						
 *																			*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,	
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF	
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND		
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR			
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,		
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,		
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER			
 * DEALINGS IN THE SOFTWARE.												
 ***************************************************************************/

#include "xgi_types.h"
#include "xgi_linux.h"
#include "xgi_drv.h"
#include "xgi_regs.h"
#include "xgi_pcie.h"

void xgi_get_device_info(struct xgi_info * info, struct xgi_chip_info * req)
{
	req->device_id = info->device_id;
	req->device_name[0] = 'x';
	req->device_name[1] = 'g';
	req->device_name[2] = '4';
	req->device_name[3] = '7';
	req->vendor_id = info->vendor_id;
	req->curr_display_mode = 0;
	req->fb_size = info->fb.size;
	req->sarea_bus_addr = info->sarea_info.bus_addr;
	req->sarea_size = info->sarea_info.size;
}

void xgi_get_mmio_info(struct xgi_info * info, struct xgi_mmio_info * req)
{
	req->mmio_base = info->mmio.base;
	req->size = info->mmio.size;
}

void xgi_put_screen_info(struct xgi_info * info, struct xgi_screen_info * req)
{
	info->scrn_info.scrn_start = req->scrn_start;
	info->scrn_info.scrn_xres = req->scrn_xres;
	info->scrn_info.scrn_yres = req->scrn_yres;
	info->scrn_info.scrn_bpp = req->scrn_bpp;
	info->scrn_info.scrn_pitch = req->scrn_pitch;

	XGI_INFO("info->scrn_info.scrn_start: 0x%lx"
		 "info->scrn_info.scrn_xres: 0x%lx"
		 "info->scrn_info.scrn_yres: 0x%lx"
		 "info->scrn_info.scrn_bpp: 0x%lx"
		 "info->scrn_info.scrn_pitch: 0x%lx\n",
		 info->scrn_info.scrn_start,
		 info->scrn_info.scrn_xres,
		 info->scrn_info.scrn_yres,
		 info->scrn_info.scrn_bpp, info->scrn_info.scrn_pitch);
}

void xgi_get_screen_info(struct xgi_info * info, struct xgi_screen_info * req)
{
	req->scrn_start = info->scrn_info.scrn_start;
	req->scrn_xres = info->scrn_info.scrn_xres;
	req->scrn_yres = info->scrn_info.scrn_yres;
	req->scrn_bpp = info->scrn_info.scrn_bpp;
	req->scrn_pitch = info->scrn_info.scrn_pitch;

	XGI_INFO("req->scrn_start: 0x%lx"
		 "req->scrn_xres: 0x%lx"
		 "req->scrn_yres: 0x%lx"
		 "req->scrn_bpp: 0x%lx"
		 "req->scrn_pitch: 0x%lx\n",
		 req->scrn_start,
		 req->scrn_xres,
		 req->scrn_yres, req->scrn_bpp, req->scrn_pitch);
}

void xgi_ge_reset(struct xgi_info * info)
{
	xgi_disable_ge(info);
	xgi_enable_ge(info);
}

void xgi_sarea_info(struct xgi_info * info, struct xgi_sarea_info * req)
{
	info->sarea_info.bus_addr = req->bus_addr;
	info->sarea_info.size = req->size;
	XGI_INFO("info->sarea_info.bus_addr: 0x%lx"
		 "info->sarea_info.size: 0x%lx\n",
		 info->sarea_info.bus_addr, info->sarea_info.size);
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

			XGI_INFO("Can not reset back 0x%x!\n",
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
	volatile u8 *const mmio_vbase = info->mmio.vbase;
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
						XGI_INFO("Reset GE!\n");

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
	u8 save_3ce = bReadReg(0x3ce);

	if (bIn3cf(0x37) & 0x01)	// CRT1 interrupt just happened
	{
		u8 op3cf_3d;
		u8 op3cf_37;

		// What happened?
		op3cf_37 = bIn3cf(0x37);

		// Clear CRT interrupt
		op3cf_3d = bIn3cf(0x3d);
		bOut3cf(0x3d, (op3cf_3d | 0x04));
		bOut3cf(0x3d, (op3cf_3d & ~0x04));
		ret = TRUE;
	}
	bWriteReg(0x3ce, save_3ce);

	return (ret);
}

bool xgi_dvi_irq_handler(struct xgi_info * info)
{
	bool ret = FALSE;
	u8 save_3ce = bReadReg(0x3ce);

	if (bIn3cf(0x38) & 0x20)	// DVI interrupt just happened
	{
		u8 op3cf_39;
		u8 op3cf_37;
		u8 op3x5_5a;
		u8 save_3x4 = bReadReg(0x3d4);;

		// What happened?
		op3cf_37 = bIn3cf(0x37);

		//Notify BIOS that DVI plug/unplug happened
		op3x5_5a = bIn3x5(0x5a);
		bOut3x5(0x5a, op3x5_5a & 0xf7);

		bWriteReg(0x3d4, save_3x4);

		// Clear DVI interrupt
		op3cf_39 = bIn3cf(0x39);
		bOut3c5(0x39, (op3cf_39 & ~0x01));	//Set 3cf.39 bit 0 to 0
		bOut3c5(0x39, (op3cf_39 | 0x01));	//Set 3cf.39 bit 0 to 1

		ret = TRUE;
	}
	bWriteReg(0x3ce, save_3ce);

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
			temp = bIn3c5(i * 0x10 + j);
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
			temp = bIn3x5(i * 0x10 + j);
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
			temp = bIn3cf(i * 0x10 + j);
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
			temp = bReadReg(0xB000 + i * 0x10 + j);
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
			temp = bReadReg(0x2200 + i * 0x10 + j);
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
			temp = bReadReg(0x2300 + i * 0x10 + j);
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
			temp = bReadReg(0x2400 + i * 0x10 + j);
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
			temp = bReadReg(0x2800 + i * 0x10 + j);
			printk("%3x", temp);
		}
		printk("\r\n");
	}
}

void xgi_restore_registers(struct xgi_info * info)
{
	bOut3x5(0x13, 0);
	bOut3x5(0x8b, 2);
}

void xgi_waitfor_pci_idle(struct xgi_info * info)
{
#define WHOLD_GE_STATUS             0x2800
#define IDLE_MASK                   ~0x90200000

	int idleCount = 0;
	while (idleCount < 5) {
		if (dwReadReg(WHOLD_GE_STATUS) & IDLE_MASK) {
			idleCount = 0;
		} else {
			idleCount++;
		}
	}
}


/*memory collect function*/
extern struct list_head xgi_mempid_list;
void xgi_mem_collect(struct xgi_info * info, unsigned int *pcnt)
{
	struct xgi_mem_pid *block;
	struct xgi_mem_pid *next;
	struct task_struct *p, *find;
	unsigned int cnt = 0;

	list_for_each_entry_safe(block, next, &xgi_mempid_list, list) {

		find = NULL;
		XGI_SCAN_PROCESS(p) {
			if (p->pid == block->pid) {
				XGI_INFO
				    ("[!]Find active pid:%ld state:%ld location:%d addr:0x%lx! \n",
				     block->pid, p->state,
				     block->location,
				     block->bus_addr);
				find = p;
				if (block->bus_addr == 0xFFFFFFFF)
					++cnt;
				break;
			}
		}
		if (!find) {
			if (block->location == XGI_MEMLOC_LOCAL) {
				XGI_INFO
				    ("Memory ProcessID free fb and delete one block pid:%ld addr:0x%lx successfully! \n",
				     block->pid, block->bus_addr);
				xgi_fb_free(info, block->bus_addr);
			} else if (block->bus_addr != 0xFFFFFFFF) {
				XGI_INFO
				    ("Memory ProcessID free pcie and delete one block pid:%ld addr:0x%lx successfully! \n",
				     block->pid, block->bus_addr);
				xgi_pcie_free(info, block->bus_addr);
			} else {
				/*only delete the memory block */
				list_del(&block->list);
				XGI_INFO
				    ("Memory ProcessID delete one pcie block pid:%ld successfully! \n",
				     block->pid);
				kfree(block);
			}
		}
	}
	*pcnt = cnt;
}

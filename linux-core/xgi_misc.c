
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

void xgi_get_device_info(xgi_info_t * info, xgi_chip_info_t * req)
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

void xgi_get_mmio_info(xgi_info_t * info, xgi_mmio_info_t * req)
{
	req->mmioBase = (void *)info->mmio.base;
	req->size = info->mmio.size;
}

void xgi_put_screen_info(xgi_info_t * info, xgi_screen_info_t * req)
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

void xgi_get_screen_info(xgi_info_t * info, xgi_screen_info_t * req)
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

void xgi_ge_reset(xgi_info_t * info)
{
	xgi_disable_ge(info);
	xgi_enable_ge(info);
}

void xgi_sarea_info(xgi_info_t * info, xgi_sarea_info_t * req)
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

static U32 s_invalid_begin = 0;

BOOL xgi_ge_irq_handler(xgi_info_t * info)
{
	volatile U8 *mmio_vbase = info->mmio.vbase;
	volatile U32 *ge_3d_status = (volatile U32 *)(mmio_vbase + 0x2800);
	U32 int_status = ge_3d_status[4];	// interrupt status
	U32 auto_reset_count = 0;
	BOOL is_support_auto_reset = FALSE;

	// Check GE on/off
	if (0 == (0xffffc0f0 & int_status)) {
		U32 old_ge_status = ge_3d_status[0x00];
		U32 old_pcie_cmd_fetch_Addr = ge_3d_status[0x0a];
		if (0 != (0x1000 & int_status)) {
			// We got GE stall interrupt.
			ge_3d_status[0x04] = int_status | 0x04000000;

			if (TRUE == is_support_auto_reset) {
				BOOL is_wrong_signal = FALSE;
				static U32 last_int_tick_low,
				    last_int_tick_high;
				static U32 new_int_tick_low, new_int_tick_high;
				static U32 continoue_int_count = 0;
				// OE II is busy.
				while (old_ge_status & 0x001c0000) {
					U16 check;
					// Check Read back status
					*(mmio_vbase + 0x235c) = 0x80;
					check =
					    *((volatile U16 *)(mmio_vbase +
							       0x2360));
					if ((check & 0x3f) !=
					    ((check & 0x3f00) >> 8)) {
						is_wrong_signal = TRUE;
						break;
					}
					// Check RO channel
					*(mmio_vbase + 0x235c) = 0x83;
					check =
					    *((volatile U16 *)(mmio_vbase +
							       0x2360));
					if ((check & 0x0f) !=
					    ((check & 0xf0) >> 4)) {
						is_wrong_signal = TRUE;
						break;
					}
					// Check RW channel
					*(mmio_vbase + 0x235c) = 0x88;
					check =
					    *((volatile U16 *)(mmio_vbase +
							       0x2360));
					if ((check & 0x0f) !=
					    ((check & 0xf0) >> 4)) {
						is_wrong_signal = TRUE;
						break;
					}
					// Check RO channel outstanding
					*(mmio_vbase + 0x235c) = 0x8f;
					check =
					    *((volatile U16 *)(mmio_vbase +
							       0x2360));
					if (0 != (check & 0x3ff)) {
						is_wrong_signal = TRUE;
						break;
					}
					// Check RW channel outstanding
					*(mmio_vbase + 0x235c) = 0x90;
					check =
					    *((volatile U16 *)(mmio_vbase +
							       0x2360));
					if (0 != (check & 0x3ff)) {
						is_wrong_signal = TRUE;
						break;
					}
					// No pending PCIE request. GE stall.
					break;
				}

				if (is_wrong_signal) {
					// Nothing but skip.
				} else if (0 == continoue_int_count++) {
					rdtsc(last_int_tick_low,
					      last_int_tick_high);
				} else {
					rdtscl(new_int_tick_low);
					if ((new_int_tick_low -
					     last_int_tick_low) >
					    STALL_INTERRUPT_RESET_THRESHOLD) {
						continoue_int_count = 0;
					} else if (continoue_int_count >= 3) {
						continoue_int_count = 0;

						// GE Hung up, need reset.
						XGI_INFO("Reset GE!\n");

						*(mmio_vbase + 0xb057) = 8;
						int time_out = 0xffff;
						while (0 !=
						       (ge_3d_status[0x00] &
							0xf0000000)) {
							while (0 !=
							       ((--time_out) &
								0xfff)) ;
							if (0 == time_out) {
								XGI_INFO
								    ("Can not reset back 0x%lx!\n",
								     ge_3d_status
								     [0x00]);
								*(mmio_vbase +
								  0xb057) = 0;
								// Have to use 3x5.36 to reset.
								// Save and close dynamic gating
								U8 old_3ce =
								    *(mmio_vbase
								      + 0x3ce);
								*(mmio_vbase +
								  0x3ce) = 0x2a;
								U8 old_3cf =
								    *(mmio_vbase
								      + 0x3cf);
								*(mmio_vbase +
								  0x3cf) =
						       old_3cf & 0xfe;
								// Reset GE
								U8 old_index =
								    *(mmio_vbase
								      + 0x3d4);
								*(mmio_vbase +
								  0x3d4) = 0x36;
								U8 old_36 =
								    *(mmio_vbase
								      + 0x3d5);
								*(mmio_vbase +
								  0x3d5) =
						       old_36 | 0x10;
								while (0 !=
								       ((--time_out) & 0xfff)) ;
								*(mmio_vbase +
								  0x3d5) =
						       old_36;
								*(mmio_vbase +
								  0x3d4) =
						       old_index;
								// Restore dynamic gating
								*(mmio_vbase +
								  0x3cf) =
						       old_3cf;
								*(mmio_vbase +
								  0x3ce) =
						       old_3ce;
								break;
							}
						}
						*(mmio_vbase + 0xb057) = 0;

						// Increase Reset counter
						auto_reset_count++;
					}
				}
			}
			return TRUE;
		} else if (0 != (0x1 & int_status)) {
			s_invalid_begin++;
			ge_3d_status[0x04] = (int_status & ~0x01) | 0x04000000;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL xgi_crt_irq_handler(xgi_info_t * info)
{
	BOOL ret = FALSE;
	U8 *mmio_vbase = info->mmio.vbase;
	U32 device_status = 0;
	U32 hw_status = 0;
	U8 save_3ce = bReadReg(0x3ce);

	if (bIn3cf(0x37) & 0x01)	// CRT1 interrupt just happened
	{
		U8 op3cf_3d;
		U8 op3cf_37;

		// What happened?
		op3cf_37 = bIn3cf(0x37);

#if 0
		if (op3cf_37 & 0x04)
			device_status |= GDEVST_CONNECT;
		else
			device_status &= ~GDEVST_CONNECT;

		device_status |= GDEVST_DEVICE_CHANGED;
		hw_status |= HWST_DEVICE_CHANGED;
#endif
		// Clear CRT interrupt
		op3cf_3d = bIn3cf(0x3d);
		bOut3cf(0x3d, (op3cf_3d | 0x04));
		bOut3cf(0x3d, (op3cf_3d & ~0x04));
		ret = TRUE;
	}
	bWriteReg(0x3ce, save_3ce);

	return (ret);
}

BOOL xgi_dvi_irq_handler(xgi_info_t * info)
{
	BOOL ret = FALSE;
	U8 *mmio_vbase = info->mmio.vbase;
	U32 device_status = 0;
	U32 hw_status = 0;
	U8 save_3ce = bReadReg(0x3ce);

	if (bIn3cf(0x38) & 0x20)	// DVI interrupt just happened
	{
		U8 op3cf_39;
		U8 op3cf_37;
		U8 op3x5_5a;
		U8 save_3x4 = bReadReg(0x3d4);;

		// What happened?
		op3cf_37 = bIn3cf(0x37);
#if 0
		//Also update our internal flag
		if (op3cf_37 & 0x10)	// Second Monitor plugged In
		{
			device_status |= GDEVST_CONNECT;
			//Because currenly we cannot determine if DVI digital
			//or DVI analog is connected according to DVI interrupt
			//We should still call BIOS to check it when utility ask us
			device_status &= ~GDEVST_CHECKED;
		} else {
			device_status &= ~GDEVST_CONNECT;
		}
#endif
		//Notify BIOS that DVI plug/unplug happened
		op3x5_5a = bIn3x5(0x5a);
		bOut3x5(0x5a, op3x5_5a & 0xf7);

		bWriteReg(0x3d4, save_3x4);

		//device_status |= GDEVST_DEVICE_CHANGED;
		//hw_status |= HWST_DEVICE_CHANGED;

		// Clear DVI interrupt
		op3cf_39 = bIn3cf(0x39);
		bOut3c5(0x39, (op3cf_39 & ~0x01));	//Set 3cf.39 bit 0 to 0
		bOut3c5(0x39, (op3cf_39 | 0x01));	//Set 3cf.39 bit 0 to 1

		ret = TRUE;
	}
	bWriteReg(0x3ce, save_3ce);

	return (ret);
}

void xgi_dump_register(xgi_info_t * info)
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

void xgi_restore_registers(xgi_info_t * info)
{
	bOut3x5(0x13, 0);
	bOut3x5(0x8b, 2);
}

void xgi_waitfor_pci_idle(xgi_info_t * info)
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

int xgi_get_cpu_id(struct cpu_info_s *arg)
{
	int op = arg->_eax;
      __asm__("cpuid":"=a"(arg->_eax),
		"=b"(arg->_ebx),
		"=c"(arg->_ecx), "=d"(arg->_edx)
      :	"0"(op));

	XGI_INFO
	    ("opCode = 0x%x, eax = 0x%x, ebx = 0x%x, ecx = 0x%x, edx = 0x%x \n",
	     op, arg->_eax, arg->_ebx, arg->_ecx, arg->_edx);
}

/*memory collect function*/
extern struct list_head xgi_mempid_list;
void xgi_mem_collect(xgi_info_t * info, unsigned int *pcnt)
{
	xgi_mem_pid_t *mempid_block;
	struct list_head *mempid_list;
	struct task_struct *p, *find;
	unsigned int cnt = 0;

	mempid_list = xgi_mempid_list.next;

	while (mempid_list != &xgi_mempid_list) {
		mempid_block =
		    list_entry(mempid_list, struct xgi_mem_pid_s, list);
		mempid_list = mempid_list->next;

		find = NULL;
		XGI_SCAN_PROCESS(p) {
			if (p->pid == mempid_block->pid) {
				XGI_INFO
				    ("[!]Find active pid:%ld state:%ld location:%d addr:0x%lx! \n",
				     mempid_block->pid, p->state,
				     mempid_block->location,
				     mempid_block->bus_addr);
				find = p;
				if (mempid_block->bus_addr == 0xFFFFFFFF)
					++cnt;
				break;
			}
		}
		if (!find) {
			if (mempid_block->location == LOCAL) {
				XGI_INFO
				    ("Memory ProcessID free fb and delete one block pid:%ld addr:0x%lx successfully! \n",
				     mempid_block->pid, mempid_block->bus_addr);
				xgi_fb_free(info, mempid_block->bus_addr);
			} else if (mempid_block->bus_addr != 0xFFFFFFFF) {
				XGI_INFO
				    ("Memory ProcessID free pcie and delete one block pid:%ld addr:0x%lx successfully! \n",
				     mempid_block->pid, mempid_block->bus_addr);
				xgi_pcie_free(info, mempid_block->bus_addr);
			} else {
				/*only delete the memory block */
				list_del(&mempid_block->list);
				XGI_INFO
				    ("Memory ProcessID delete one pcie block pid:%ld successfully! \n",
				     mempid_block->pid);
				kfree(mempid_block);
			}
		}
	}
	*pcnt = cnt;
}

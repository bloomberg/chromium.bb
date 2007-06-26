
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


#ifndef _XGI_MISC_H_
#define _XGI_MISC_H_

extern void xgi_dump_register(xgi_info_t *info);
extern void xgi_get_device_info(xgi_info_t *info, xgi_chip_info_t * req);
extern void xgi_get_mmio_info(xgi_info_t *info, xgi_mmio_info_t *req);
extern void xgi_get_screen_info(xgi_info_t *info, xgi_screen_info_t *req);
extern void xgi_put_screen_info(xgi_info_t *info, xgi_screen_info_t *req);
extern void xgi_ge_reset(xgi_info_t *info);
extern void xgi_sarea_info(xgi_info_t *info, xgi_sarea_info_t *req);
extern int  xgi_get_cpu_id(struct cpu_info_s *arg);

extern void xgi_restore_registers(xgi_info_t *info);
extern BOOL xgi_ge_irq_handler(xgi_info_t *info);
extern BOOL xgi_crt_irq_handler(xgi_info_t *info);
extern BOOL xgi_dvi_irq_handler(xgi_info_t *info);
extern void xgi_waitfor_pci_idle(xgi_info_t *info);


#endif

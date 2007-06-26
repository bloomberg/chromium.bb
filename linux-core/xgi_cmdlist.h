
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

#ifndef _XGI_CMDLIST_H_
#define _XGI_CMDLIST_H_

#define		ONE_BIT_MASK							0x1
#define		TWENTY_BIT_MASK							0xfffff
#define 	M2REG_FLUSH_2D_ENGINE_MASK				(ONE_BIT_MASK<<20)
#define 	M2REG_FLUSH_3D_ENGINE_MASK				TWENTY_BIT_MASK
#define 	M2REG_FLUSH_FLIP_ENGINE_MASK			(ONE_BIT_MASK<<21)
#define     BASE_3D_ENG                             0x2800
#define     M2REG_AUTO_LINK_SETTING_ADDRESS         0x10
#define 	M2REG_CLEAR_COUNTERS_MASK				(ONE_BIT_MASK<<4)
#define 	M2REG_PCI_TRIGGER_MODE_MASK				(ONE_BIT_MASK<<1)
#define 	BEGIN_VALID_MASK                        (ONE_BIT_MASK<<20)
#define 	BEGIN_LINK_ENABLE_MASK                  (ONE_BIT_MASK<<31)
#define     M2REG_PCI_TRIGGER_REGISTER_ADDRESS      0x14

typedef enum
{
    FLUSH_2D			= M2REG_FLUSH_2D_ENGINE_MASK,
    FLUSH_3D			= M2REG_FLUSH_3D_ENGINE_MASK,
    FLUSH_FLIP			= M2REG_FLUSH_FLIP_ENGINE_MASK
}FLUSH_CODE;

typedef enum
{
    AGPCMDLIST_SCRATCH_SIZE         = 0x100,
    AGPCMDLIST_BEGIN_SIZE           = 0x004,
    AGPCMDLIST_3D_SCRATCH_CMD_SIZE  = 0x004,
    AGPCMDLIST_2D_SCRATCH_CMD_SIZE  = 0x00c,
    AGPCMDLIST_FLUSH_CMD_LEN        = 0x004,
    AGPCMDLIST_DUMY_END_BATCH_LEN   = AGPCMDLIST_BEGIN_SIZE
}CMD_SIZE;

typedef struct xgi_cmdring_info_s
{
	U32		_cmdRingSize;
	U32     _cmdRingBuffer;
    U32     _cmdRingBusAddr;
	U32     _lastBatchStartAddr;
    U32     _cmdRingOffset;
}xgi_cmdring_info_t;

extern int xgi_cmdlist_initialize(xgi_info_t *info, U32 size);

extern void xgi_submit_cmdlist(xgi_info_t *info, xgi_cmd_info_t * pCmdInfo);

extern void xgi_state_change(xgi_info_t *info, xgi_state_info_t * pStateInfo);

extern void xgi_cmdlist_cleanup(xgi_info_t *info);

#endif /* _XGI_CMDLIST_H_ */

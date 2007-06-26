
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
#include "xgi_misc.h"
#include "xgi_cmdlist.h"

U32 s_emptyBegin[AGPCMDLIST_BEGIN_SIZE] = {
	0x10000000,		// 3D Type Begin, Invalid
	0x80000004,		// Length = 4;
	0x00000000,
	0x00000000
};

U32 s_flush2D[AGPCMDLIST_FLUSH_CMD_LEN] = {
	FLUSH_2D,
	FLUSH_2D,
	FLUSH_2D,
	FLUSH_2D
};

xgi_cmdring_info_t s_cmdring;

static void addFlush2D(xgi_info_t * info);
static U32 getCurBatchBeginPort(xgi_cmd_info_t * pCmdInfo);
static void triggerHWCommandList(xgi_info_t * info, U32 triggerCounter);
static void xgi_cmdlist_reset(void);

int xgi_cmdlist_initialize(xgi_info_t * info, U32 size)
{
	//xgi_mem_req_t mem_req;
	xgi_mem_alloc_t mem_alloc;

	//mem_req.size = size;

	xgi_pcie_alloc(info, size, PCIE_2D, &mem_alloc);

	if ((mem_alloc.size == 0) && (mem_alloc.hw_addr == 0)) {
		return -1;
	}

	s_cmdring._cmdRingSize = mem_alloc.size;
	s_cmdring._cmdRingBuffer = mem_alloc.hw_addr;
	s_cmdring._cmdRingBusAddr = mem_alloc.bus_addr;
	s_cmdring._lastBatchStartAddr = 0;
	s_cmdring._cmdRingOffset = 0;

	return 1;
}

void xgi_submit_cmdlist(xgi_info_t * info, xgi_cmd_info_t * pCmdInfo)
{
	U32 beginPort;
    /** XGI_INFO("Jong-xgi_submit_cmdlist-Begin \n"); **/

	/* Jong 05/25/2006 */
	/* return; */

	beginPort = getCurBatchBeginPort(pCmdInfo);
	XGI_INFO("Jong-xgi_submit_cmdlist-After getCurBatchBeginPort() \n");

	/* Jong 05/25/2006 */
	/* return; */

	if (s_cmdring._lastBatchStartAddr == 0) {
		U32 portOffset;

		/* Jong 06/13/2006; remove marked for system hang test */
		/* xgi_waitfor_pci_idle(info); */

		/* Jong 06132006; BASE_3D_ENG=0x2800 */
		/* beginPort: 2D: 0x30 */
		portOffset = BASE_3D_ENG + beginPort;

		// Enable PCI Trigger Mode
		XGI_INFO("Jong-xgi_submit_cmdlist-Enable PCI Trigger Mode \n");

		/* Jong 05/25/2006 */
		/* return; */

		/* Jong 06/13/2006; M2REG_AUTO_LINK_SETTING_ADDRESS=0x10 */
		XGI_INFO("Jong-M2REG_AUTO_LINK_SETTING_ADDRESS=0x%lx \n",
			 M2REG_AUTO_LINK_SETTING_ADDRESS);
		XGI_INFO("Jong-M2REG_CLEAR_COUNTERS_MASK=0x%lx \n",
			 M2REG_CLEAR_COUNTERS_MASK);
		XGI_INFO
		    ("Jong-(M2REG_AUTO_LINK_SETTING_ADDRESS << 22)=0x%lx \n",
		     (M2REG_AUTO_LINK_SETTING_ADDRESS << 22));
		XGI_INFO("Jong-M2REG_PCI_TRIGGER_MODE_MASK=0x%lx \n\n",
			 M2REG_PCI_TRIGGER_MODE_MASK);

		/* Jong 06/14/2006; 0x400001a */
		XGI_INFO
		    ("Jong-(M2REG_AUTO_LINK_SETTING_ADDRESS << 22)|M2REG_CLEAR_COUNTERS_MASK|0x08|M2REG_PCI_TRIGGER_MODE_MASK=0x%lx \n",
		     (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) |
		     M2REG_CLEAR_COUNTERS_MASK | 0x08 |
		     M2REG_PCI_TRIGGER_MODE_MASK);
		dwWriteReg(BASE_3D_ENG + M2REG_AUTO_LINK_SETTING_ADDRESS,
			   (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) |
			   M2REG_CLEAR_COUNTERS_MASK | 0x08 |
			   M2REG_PCI_TRIGGER_MODE_MASK);

		/* Jong 05/25/2006 */
		XGI_INFO("Jong-xgi_submit_cmdlist-After dwWriteReg() \n");
		/* return; *//* OK */

		/* Jong 06/14/2006; 0x400000a */
		XGI_INFO
		    ("Jong-(M2REG_AUTO_LINK_SETTING_ADDRESS << 22)|0x08|M2REG_PCI_TRIGGER_MODE_MASK=0x%lx \n",
		     (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) | 0x08 |
		     M2REG_PCI_TRIGGER_MODE_MASK);
		dwWriteReg(BASE_3D_ENG + M2REG_AUTO_LINK_SETTING_ADDRESS,
			   (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) | 0x08 |
			   M2REG_PCI_TRIGGER_MODE_MASK);

		// Send PCI begin command
		XGI_INFO("Jong-xgi_submit_cmdlist-Send PCI begin command \n");
		/* return; */

		XGI_INFO("Jong-xgi_submit_cmdlist-portOffset=%d \n",
			 portOffset);
		XGI_INFO("Jong-xgi_submit_cmdlist-beginPort=%d \n", beginPort);

		/* beginPort = 48; */
		/* 0xc100000 */
		dwWriteReg(portOffset,
			   (beginPort << 22) + (BEGIN_VALID_MASK) +
			   pCmdInfo->_curDebugID);
		XGI_INFO("Jong-(beginPort<<22)=0x%lx \n", (beginPort << 22));
		XGI_INFO("Jong-(BEGIN_VALID_MASK)=0x%lx \n", BEGIN_VALID_MASK);
		XGI_INFO("Jong- pCmdInfo->_curDebugID=0x%lx \n",
			 pCmdInfo->_curDebugID);
		XGI_INFO
		    ("Jong- (beginPort<<22) + (BEGIN_VALID_MASK) + pCmdInfo->_curDebugID=0x%lx \n",
		     (beginPort << 22) + (BEGIN_VALID_MASK) +
		     pCmdInfo->_curDebugID);
		XGI_INFO
		    ("Jong-xgi_submit_cmdlist-Send PCI begin command- After \n");
		/* return; *//* OK */

		/* 0x80000024 */
		dwWriteReg(portOffset + 4,
			   BEGIN_LINK_ENABLE_MASK + pCmdInfo->_firstSize);
		XGI_INFO("Jong- BEGIN_LINK_ENABLE_MASK=0x%lx \n",
			 BEGIN_LINK_ENABLE_MASK);
		XGI_INFO("Jong- pCmdInfo->_firstSize=0x%lx \n",
			 pCmdInfo->_firstSize);
		XGI_INFO
		    ("Jong-  BEGIN_LINK_ENABLE_MASK + pCmdInfo->_firstSize=0x%lx \n",
		     BEGIN_LINK_ENABLE_MASK + pCmdInfo->_firstSize);
		XGI_INFO("Jong-xgi_submit_cmdlist-dwWriteReg-1 \n");

		/* 0x1010000 */
		dwWriteReg(portOffset + 8, (pCmdInfo->_firstBeginAddr >> 4));
		XGI_INFO("Jong-  pCmdInfo->_firstBeginAddr=0x%lx \n",
			 pCmdInfo->_firstBeginAddr);
		XGI_INFO("Jong-  (pCmdInfo->_firstBeginAddr >> 4)=0x%lx \n",
			 (pCmdInfo->_firstBeginAddr >> 4));
		XGI_INFO("Jong-xgi_submit_cmdlist-dwWriteReg-2 \n");

		/* Jong 06/13/2006 */
		xgi_dump_register(info);

		/* Jong 06/12/2006; system hang; marked for test */
		dwWriteReg(portOffset + 12, 0);
		XGI_INFO("Jong-xgi_submit_cmdlist-dwWriteReg-3 \n");

		/* Jong 06/13/2006; remove marked for system hang test */
		/* xgi_waitfor_pci_idle(info); */
	} else {
		U32 *lastBatchVirtAddr;

		XGI_INFO
		    ("Jong-xgi_submit_cmdlist-s_cmdring._lastBatchStartAddr != 0 \n");

		/* Jong 05/25/2006 */
		/* return; */

		if (pCmdInfo->_firstBeginType == BTYPE_3D) {
			addFlush2D(info);
		}

		lastBatchVirtAddr =
		    (U32 *) xgi_find_pcie_virt(info,
					       s_cmdring._lastBatchStartAddr);

		lastBatchVirtAddr[1] =
		    BEGIN_LINK_ENABLE_MASK + pCmdInfo->_firstSize;
		lastBatchVirtAddr[2] = pCmdInfo->_firstBeginAddr >> 4;
		lastBatchVirtAddr[3] = 0;
		//barrier();
		lastBatchVirtAddr[0] =
		    (beginPort << 22) + (BEGIN_VALID_MASK) +
		    (0xffff & pCmdInfo->_curDebugID);

		/* Jong 06/12/2006; system hang; marked for test */
		triggerHWCommandList(info, pCmdInfo->_beginCount);

		XGI_INFO
		    ("Jong-xgi_submit_cmdlist-s_cmdring._lastBatchStartAddr != 0 - End\n");
	}

	s_cmdring._lastBatchStartAddr = pCmdInfo->_lastBeginAddr;
	XGI_INFO("Jong-xgi_submit_cmdlist-End \n");
}

/*
    state:      0 - console
                1 - graphic
                2 - fb
                3 - logout
*/
void xgi_state_change(xgi_info_t * info, xgi_state_info_t * pStateInfo)
{
#define STATE_CONSOLE   0
#define STATE_GRAPHIC   1
#define STATE_FBTERM    2
#define STATE_LOGOUT    3
#define STATE_REBOOT    4
#define STATE_SHUTDOWN  5

	if ((pStateInfo->_fromState == STATE_GRAPHIC)
	    && (pStateInfo->_toState == STATE_CONSOLE)) {
		XGI_INFO("[kd] I see, now is to leaveVT\n");
		// stop to received batch
	} else if ((pStateInfo->_fromState == STATE_CONSOLE)
		   && (pStateInfo->_toState == STATE_GRAPHIC)) {
		XGI_INFO("[kd] I see, now is to enterVT\n");
		xgi_cmdlist_reset();
	} else if ((pStateInfo->_fromState == STATE_GRAPHIC)
		   && ((pStateInfo->_toState == STATE_LOGOUT)
		       || (pStateInfo->_toState == STATE_REBOOT)
		       || (pStateInfo->_toState == STATE_SHUTDOWN))) {
		XGI_INFO("[kd] I see, not is to exit from X\n");
		// stop to received batch
	} else {
		XGI_ERROR("[kd] Should not happen\n");
	}

}

void xgi_cmdlist_reset(void)
{
	s_cmdring._lastBatchStartAddr = 0;
	s_cmdring._cmdRingOffset = 0;
}

void xgi_cmdlist_cleanup(xgi_info_t * info)
{
	if (s_cmdring._cmdRingBuffer != 0) {
		xgi_pcie_free(info, s_cmdring._cmdRingBusAddr);
		s_cmdring._cmdRingBuffer = 0;
		s_cmdring._cmdRingOffset = 0;
		s_cmdring._cmdRingSize = 0;
	}
}

static void triggerHWCommandList(xgi_info_t * info, U32 triggerCounter)
{
	static U32 s_triggerID = 1;

	//Fix me, currently we just trigger one time
	while (triggerCounter--) {
		dwWriteReg(BASE_3D_ENG + M2REG_PCI_TRIGGER_REGISTER_ADDRESS,
			   0x05000000 + (0xffff & s_triggerID++));
		// xgi_waitfor_pci_idle(info);
	}
}

static U32 getCurBatchBeginPort(xgi_cmd_info_t * pCmdInfo)
{
	// Convert the batch type to begin port ID
	switch (pCmdInfo->_firstBeginType) {
	case BTYPE_2D:
		return 0x30;
	case BTYPE_3D:
		return 0x40;
	case BTYPE_FLIP:
		return 0x50;
	case BTYPE_CTRL:
		return 0x20;
	default:
		//ASSERT(0);
		return 0xff;
	}
}

static void addFlush2D(xgi_info_t * info)
{
	U32 *flushBatchVirtAddr;
	U32 flushBatchHWAddr;

	U32 *lastBatchVirtAddr;

	/* check buf is large enough to contain a new flush batch */
	if ((s_cmdring._cmdRingOffset + 0x20) >= s_cmdring._cmdRingSize) {
		s_cmdring._cmdRingOffset = 0;
	}

	flushBatchHWAddr = s_cmdring._cmdRingBuffer + s_cmdring._cmdRingOffset;
	flushBatchVirtAddr = (U32 *) xgi_find_pcie_virt(info, flushBatchHWAddr);

	/* not using memcpy for I assume the address is discrete */
	*(flushBatchVirtAddr + 0) = 0x10000000;
	*(flushBatchVirtAddr + 1) = 0x80000004;	/* size = 0x04 dwords */
	*(flushBatchVirtAddr + 2) = 0x00000000;
	*(flushBatchVirtAddr + 3) = 0x00000000;
	*(flushBatchVirtAddr + 4) = FLUSH_2D;
	*(flushBatchVirtAddr + 5) = FLUSH_2D;
	*(flushBatchVirtAddr + 6) = FLUSH_2D;
	*(flushBatchVirtAddr + 7) = FLUSH_2D;

	// ASSERT(s_cmdring._lastBatchStartAddr != NULL);
	lastBatchVirtAddr =
	    (U32 *) xgi_find_pcie_virt(info, s_cmdring._lastBatchStartAddr);

	lastBatchVirtAddr[1] = BEGIN_LINK_ENABLE_MASK + 0x08;
	lastBatchVirtAddr[2] = flushBatchHWAddr >> 4;
	lastBatchVirtAddr[3] = 0;

	//barrier();

	// BTYPE_CTRL & NO debugID
	lastBatchVirtAddr[0] = (0x20 << 22) + (BEGIN_VALID_MASK);

	triggerHWCommandList(info, 1);

	s_cmdring._cmdRingOffset += 0x20;
	s_cmdring._lastBatchStartAddr = flushBatchHWAddr;
}

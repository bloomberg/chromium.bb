
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

#include "xgi_drv.h"
#include "xgi_regs.h"
#include "xgi_misc.h"
#include "xgi_cmdlist.h"

struct xgi_cmdring_info s_cmdring;

static void addFlush2D(struct xgi_info * info);
static unsigned int getCurBatchBeginPort(struct xgi_cmd_info * pCmdInfo);
static void triggerHWCommandList(struct xgi_info * info,
				 unsigned int triggerCounter);
static void xgi_cmdlist_reset(void);

int xgi_cmdlist_initialize(struct xgi_info * info, size_t size)
{
	struct xgi_mem_alloc mem_alloc = {
		.size = size,
		.owner = PCIE_2D,
	};

	xgi_pcie_alloc(info, &mem_alloc, 0);

	if ((mem_alloc.size == 0) && (mem_alloc.hw_addr == 0)) {
		return -1;
	}

	s_cmdring._cmdRingSize = mem_alloc.size;
	s_cmdring._cmdRingBuffer = mem_alloc.hw_addr;
	s_cmdring._cmdRingAllocOffset = mem_alloc.offset;
	s_cmdring._lastBatchStartAddr = 0;
	s_cmdring._cmdRingOffset = 0;

	return 1;
}

static void xgi_submit_cmdlist(struct xgi_info * info,
			       struct xgi_cmd_info * pCmdInfo)
{
	const unsigned int beginPort = getCurBatchBeginPort(pCmdInfo);

	DRM_INFO("After getCurBatchBeginPort()\n");

	if (s_cmdring._lastBatchStartAddr == 0) {
		const unsigned int portOffset = BASE_3D_ENG + beginPort;

		/* Jong 06/13/2006; remove marked for system hang test */
		/* xgi_waitfor_pci_idle(info); */

		// Enable PCI Trigger Mode
		DRM_INFO("Enable PCI Trigger Mode \n");


		/* Jong 06/14/2006; 0x400001a */
		dwWriteReg(info->mmio_map,
			   BASE_3D_ENG + M2REG_AUTO_LINK_SETTING_ADDRESS,
			   (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) |
			   M2REG_CLEAR_COUNTERS_MASK | 0x08 |
			   M2REG_PCI_TRIGGER_MODE_MASK);

		/* Jong 06/14/2006; 0x400000a */
		dwWriteReg(info->mmio_map,
			   BASE_3D_ENG + M2REG_AUTO_LINK_SETTING_ADDRESS,
			   (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) | 0x08 |
			   M2REG_PCI_TRIGGER_MODE_MASK);

		// Send PCI begin command
		DRM_INFO("Send PCI begin command \n");

		DRM_INFO("portOffset=%d, beginPort=%d\n",
			 portOffset, beginPort);

		/* beginPort = 48; */
		/* 0xc100000 */
		dwWriteReg(info->mmio_map, portOffset,
			   (beginPort << 22) + (BEGIN_VALID_MASK) +
			   pCmdInfo->_curDebugID);

		DRM_INFO("Send PCI begin command- After\n");

		/* 0x80000024 */
		dwWriteReg(info->mmio_map, portOffset + 4,
			   BEGIN_LINK_ENABLE_MASK + pCmdInfo->_firstSize);

		/* 0x1010000 */
		dwWriteReg(info->mmio_map, portOffset + 8, 
			   (pCmdInfo->_firstBeginAddr >> 4));

		/* Jong 06/12/2006; system hang; marked for test */
		dwWriteReg(info->mmio_map, portOffset + 12, 0);

		/* Jong 06/13/2006; remove marked for system hang test */
		/* xgi_waitfor_pci_idle(info); */
	} else {
		u32 *lastBatchVirtAddr;

		DRM_INFO("s_cmdring._lastBatchStartAddr != 0\n");

		if (pCmdInfo->_firstBeginType == BTYPE_3D) {
			addFlush2D(info);
		}

		lastBatchVirtAddr = 
			xgi_find_pcie_virt(info,
					   s_cmdring._lastBatchStartAddr);

		/* lastBatchVirtAddr should *never* be NULL.  However, there
		 * are currently some bugs that cause this to happen.  The
		 * if-statement here prevents some fatal (i.e., hard lock
		 * requiring the reset button) oopses.
		 */
		if (lastBatchVirtAddr) {
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
		} else {
			DRM_ERROR("lastBatchVirtAddr is NULL\n");
		}
	}

	s_cmdring._lastBatchStartAddr = pCmdInfo->_lastBeginAddr;
	DRM_INFO("End\n");
}


int xgi_submit_cmdlist_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	struct xgi_cmd_info  cmd_list;
	struct xgi_info *info = dev->dev_private;

	DRM_COPY_FROM_USER_IOCTL(cmd_list, 
				 (struct xgi_cmd_info __user *) data,
				 sizeof(cmd_list));

	xgi_submit_cmdlist(info, &cmd_list);
	return 0;
}


/*
    state:      0 - console
                1 - graphic
                2 - fb
                3 - logout
*/
int xgi_state_change(struct xgi_info * info, unsigned int to, 
		     unsigned int from)
{
#define STATE_CONSOLE   0
#define STATE_GRAPHIC   1
#define STATE_FBTERM    2
#define STATE_LOGOUT    3
#define STATE_REBOOT    4
#define STATE_SHUTDOWN  5

	if ((from == STATE_GRAPHIC) && (to == STATE_CONSOLE)) {
		DRM_INFO("[kd] I see, now is to leaveVT\n");
		// stop to received batch
	} else if ((from == STATE_CONSOLE) && (to == STATE_GRAPHIC)) {
		DRM_INFO("[kd] I see, now is to enterVT\n");
		xgi_cmdlist_reset();
	} else if ((from == STATE_GRAPHIC)
		   && ((to == STATE_LOGOUT)
		       || (to == STATE_REBOOT)
		       || (to == STATE_SHUTDOWN))) {
		DRM_INFO("[kd] I see, not is to exit from X\n");
		// stop to received batch
	} else {
		DRM_ERROR("[kd] Should not happen\n");
		return DRM_ERR(EINVAL);
	}

	return 0;
}


int xgi_state_change_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	struct xgi_state_info  state;
	struct xgi_info *info = dev->dev_private;

	DRM_COPY_FROM_USER_IOCTL(state, (struct xgi_state_info __user *) data,
				 sizeof(state));

	return xgi_state_change(info, state._toState, state._fromState);
}


void xgi_cmdlist_reset(void)
{
	s_cmdring._lastBatchStartAddr = 0;
	s_cmdring._cmdRingOffset = 0;
}

void xgi_cmdlist_cleanup(struct xgi_info * info)
{
	if (s_cmdring._cmdRingBuffer != 0) {
		xgi_pcie_free(info, s_cmdring._cmdRingAllocOffset, NULL);
		s_cmdring._cmdRingBuffer = 0;
		s_cmdring._cmdRingOffset = 0;
		s_cmdring._cmdRingSize = 0;
	}
}

static void triggerHWCommandList(struct xgi_info * info,
				 unsigned int triggerCounter)
{
	static unsigned int s_triggerID = 1;

	//Fix me, currently we just trigger one time
	while (triggerCounter--) {
		dwWriteReg(info->mmio_map,
			   BASE_3D_ENG + M2REG_PCI_TRIGGER_REGISTER_ADDRESS,
			   0x05000000 + (0x0ffff & s_triggerID++));
		// xgi_waitfor_pci_idle(info);
	}
}

static unsigned int getCurBatchBeginPort(struct xgi_cmd_info * pCmdInfo)
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

static void addFlush2D(struct xgi_info * info)
{
	u32 *flushBatchVirtAddr;
	u32 flushBatchHWAddr;
	u32 *lastBatchVirtAddr;

	/* check buf is large enough to contain a new flush batch */
	if ((s_cmdring._cmdRingOffset + 0x20) >= s_cmdring._cmdRingSize) {
		s_cmdring._cmdRingOffset = 0;
	}

	flushBatchHWAddr = s_cmdring._cmdRingBuffer + s_cmdring._cmdRingOffset;
	flushBatchVirtAddr = xgi_find_pcie_virt(info, flushBatchHWAddr);

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
		xgi_find_pcie_virt(info, s_cmdring._lastBatchStartAddr);

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

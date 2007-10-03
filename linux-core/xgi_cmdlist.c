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
#include "xgi_misc.h"
#include "xgi_cmdlist.h"

static void xgi_emit_flush(struct xgi_info * info, bool stop);
static void xgi_emit_nop(struct xgi_info * info);
static unsigned int get_batch_command(enum xgi_batch_type type);
static void triggerHWCommandList(struct xgi_info * info);
static void xgi_cmdlist_reset(struct xgi_info * info);


/**
 * Graphic engine register (2d/3d) acessing interface
 */
static inline void dwWriteReg(struct drm_map * map, u32 addr, u32 data)
{
#ifdef XGI_MMIO_DEBUG
	DRM_INFO("mmio_map->handle = 0x%p, addr = 0x%x, data = 0x%x\n",
		 map->handle, addr, data);
#endif
	DRM_WRITE32(map, addr, cpu_to_le32(data));
}


int xgi_cmdlist_initialize(struct xgi_info * info, size_t size,
			   struct drm_file * filp)
{
	struct xgi_mem_alloc mem_alloc = {
		.location = XGI_MEMLOC_NON_LOCAL,
		.size = size,
	};
	int err;

	err = xgi_alloc(info, &mem_alloc, filp);
	if (err) {
		return err;
	}

	info->cmdring.ptr = xgi_find_pcie_virt(info, mem_alloc.hw_addr);
	info->cmdring.size = mem_alloc.size;
	info->cmdring.ring_hw_base = mem_alloc.hw_addr;
	info->cmdring.last_ptr = NULL;
	info->cmdring.ring_offset = 0;

	return 0;
}


/**
 * get_batch_command - Get the command ID for the current begin type.
 * @type: Type of the current batch
 *
 * See section 3.2.2 "Begin" (page 15) of the 3D SPG.
 * 
 * This function assumes that @type is on the range [0,3].
 */
unsigned int get_batch_command(enum xgi_batch_type type)
{
	static const unsigned int ports[4] = {
		0x30 >> 2, 0x40 >> 2, 0x50 >> 2, 0x20 >> 2
	};
	
	return ports[type];
}


int xgi_submit_cmdlist(struct drm_device * dev, void * data,
		       struct drm_file * filp)
{
	struct xgi_info *const info = dev->dev_private;
	const struct xgi_cmd_info *const pCmdInfo =
		(struct xgi_cmd_info *) data;
	const unsigned int cmd = get_batch_command(pCmdInfo->type);
#ifdef __BIG_ENDIAN
	const u32 *const ptr = xgi_find_pcie_virt(info, pCmdInfo->hw_addr);
	unsigned i;
	unsigned j;

	xgi_waitfor_pci_idle(info);
	for (j = 4; j < pCmdInfo->size; j += 4) {
		u32 reg = ptr[j];

		for (i = 1; i < 4; i++) {
			if ((reg & 1) != 0) {
				const unsigned r = 0x2100 | (reg & 0x0fe);
				DRM_WRITE32(info->mmio_map, r, ptr[j + i]);
			}

			reg >>= 8;
		}
	}
#else
	u32 begin[4];


	begin[0] = (cmd << 24) | BEGIN_VALID_MASK
		| (BEGIN_BEGIN_IDENTIFICATION_MASK & info->next_sequence);
	begin[1] = BEGIN_LINK_ENABLE_MASK | pCmdInfo->size;
	begin[2] = pCmdInfo->hw_addr >> 4;
	begin[3] = 0;

	if (info->cmdring.last_ptr == NULL) {
		const unsigned int portOffset = BASE_3D_ENG + (cmd << 2);


		/* Enable PCI Trigger Mode
		 */
		dwWriteReg(info->mmio_map,
			   BASE_3D_ENG + M2REG_AUTO_LINK_SETTING_ADDRESS,
			   (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) |
			   M2REG_CLEAR_COUNTERS_MASK | 0x08 |
			   M2REG_PCI_TRIGGER_MODE_MASK);

		dwWriteReg(info->mmio_map,
			   BASE_3D_ENG + M2REG_AUTO_LINK_SETTING_ADDRESS,
			   (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) | 0x08 |
			   M2REG_PCI_TRIGGER_MODE_MASK);


		/* Send PCI begin command
		 */
		dwWriteReg(info->mmio_map, portOffset,      begin[0]);
		dwWriteReg(info->mmio_map, portOffset +  4, begin[1]);
		dwWriteReg(info->mmio_map, portOffset +  8, begin[2]);
		dwWriteReg(info->mmio_map, portOffset + 12, begin[3]);
	} else {
		DRM_DEBUG("info->cmdring.last_ptr != NULL\n");

		if (pCmdInfo->type == BTYPE_3D) {
			xgi_emit_flush(info, FALSE);
		}

		info->cmdring.last_ptr[1] = cpu_to_le32(begin[1]);
		info->cmdring.last_ptr[2] = cpu_to_le32(begin[2]);
		info->cmdring.last_ptr[3] = cpu_to_le32(begin[3]);
		DRM_WRITEMEMORYBARRIER();
		info->cmdring.last_ptr[0] = cpu_to_le32(begin[0]);

		triggerHWCommandList(info);
	}

	info->cmdring.last_ptr = xgi_find_pcie_virt(info, pCmdInfo->hw_addr);
#endif
	drm_fence_flush_old(info->dev, 0, info->next_sequence);
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
		DRM_INFO("Leaving graphical mode (probably VT switch)\n");
	} else if ((from == STATE_CONSOLE) && (to == STATE_GRAPHIC)) {
		DRM_INFO("Entering graphical mode (probably VT switch)\n");
		xgi_cmdlist_reset(info);
	} else if ((from == STATE_GRAPHIC)
		   && ((to == STATE_LOGOUT)
		       || (to == STATE_REBOOT)
		       || (to == STATE_SHUTDOWN))) {
		DRM_INFO("Leaving graphical mode (probably X shutting down)\n");
	} else {
		DRM_ERROR("Invalid state change.\n");
		return -EINVAL;
	}

	return 0;
}


int xgi_state_change_ioctl(struct drm_device * dev, void * data,
			   struct drm_file * filp)
{
	struct xgi_state_info *const state =
		(struct xgi_state_info *) data;
	struct xgi_info *info = dev->dev_private;


	return xgi_state_change(info, state->_toState, state->_fromState);
}


void xgi_cmdlist_reset(struct xgi_info * info)
{
	info->cmdring.last_ptr = NULL;
	info->cmdring.ring_offset = 0;
}


void xgi_cmdlist_cleanup(struct xgi_info * info)
{
	if (info->cmdring.ring_hw_base != 0) {
		/* If command lists have been issued, terminate the command
		 * list chain with a flush command.
		 */
		if (info->cmdring.last_ptr != NULL) {
			xgi_emit_flush(info, FALSE);
			xgi_emit_nop(info);
		}

		xgi_waitfor_pci_idle(info);
		
		(void) memset(&info->cmdring, 0, sizeof(info->cmdring));
	}
}

static void triggerHWCommandList(struct xgi_info * info)
{
	static unsigned int s_triggerID = 1;

	dwWriteReg(info->mmio_map,
		   BASE_3D_ENG + M2REG_PCI_TRIGGER_REGISTER_ADDRESS,
		   0x05000000 + (0x0ffff & s_triggerID++));
}


/**
 * Emit a flush to the CRTL command stream.
 * @info XGI info structure
 *
 * This function assumes info->cmdring.ptr is non-NULL.
 */
void xgi_emit_flush(struct xgi_info * info, bool stop)
{
	const u32 flush_command[8] = {
		((0x10 << 24) 
		 | (BEGIN_BEGIN_IDENTIFICATION_MASK & info->next_sequence)),
		BEGIN_LINK_ENABLE_MASK | (0x00004),
		0x00000000, 0x00000000,

		/* Flush the 2D engine with the default 32 clock delay.
		 */
		M2REG_FLUSH_ENGINE_COMMAND | M2REG_FLUSH_2D_ENGINE_MASK,
		M2REG_FLUSH_ENGINE_COMMAND | M2REG_FLUSH_2D_ENGINE_MASK,
		M2REG_FLUSH_ENGINE_COMMAND | M2REG_FLUSH_2D_ENGINE_MASK,
		M2REG_FLUSH_ENGINE_COMMAND | M2REG_FLUSH_2D_ENGINE_MASK,
	};
	const unsigned int flush_size = sizeof(flush_command);
	u32 *batch_addr;
	u32 hw_addr;
	unsigned int i;


	/* check buf is large enough to contain a new flush batch */
	if ((info->cmdring.ring_offset + flush_size) >= info->cmdring.size) {
		info->cmdring.ring_offset = 0;
	}

	hw_addr = info->cmdring.ring_hw_base 
		+ info->cmdring.ring_offset;
	batch_addr = info->cmdring.ptr 
		+ (info->cmdring.ring_offset / 4);

	for (i = 0; i < (flush_size / 4); i++) {
		batch_addr[i] = cpu_to_le32(flush_command[i]);
	}

	if (stop) {
		*batch_addr |= cpu_to_le32(BEGIN_STOP_STORE_CURRENT_POINTER_MASK);
	}

	info->cmdring.last_ptr[1] = cpu_to_le32(BEGIN_LINK_ENABLE_MASK | (flush_size / 4));
	info->cmdring.last_ptr[2] = cpu_to_le32(hw_addr >> 4);
	info->cmdring.last_ptr[3] = 0;
	DRM_WRITEMEMORYBARRIER();
	info->cmdring.last_ptr[0] = cpu_to_le32((get_batch_command(BTYPE_CTRL) << 24)
		| (BEGIN_VALID_MASK));

	triggerHWCommandList(info);

	info->cmdring.ring_offset += flush_size;
	info->cmdring.last_ptr = batch_addr;
}


/**
 * Emit an empty command to the CRTL command stream.
 * @info XGI info structure
 *
 * This function assumes info->cmdring.ptr is non-NULL.  In addition, since
 * this function emits a command that does not have linkage information,
 * it sets info->cmdring.ptr to NULL.
 */
void xgi_emit_nop(struct xgi_info * info)
{
	info->cmdring.last_ptr[1] = BEGIN_LINK_ENABLE_MASK 
		| (BEGIN_BEGIN_IDENTIFICATION_MASK & info->next_sequence);
	info->cmdring.last_ptr[2] = 0;
	info->cmdring.last_ptr[3] = 0;
	DRM_WRITEMEMORYBARRIER();
	info->cmdring.last_ptr[0] = (get_batch_command(BTYPE_CTRL) << 24) 
		| (BEGIN_VALID_MASK);

	triggerHWCommandList(info);

	info->cmdring.last_ptr = NULL;
}


void xgi_emit_irq(struct xgi_info * info)
{
	if (info->cmdring.last_ptr == NULL)
		return;

	xgi_emit_flush(info, TRUE);
}

/* proc.c -- /proc support for DRM -*- c -*-
 * Created: Mon Jan 11 09:48:47 1999 by faith@precisioninsight.com
 * Revised: Fri Aug 20 11:31:48 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * $PI$
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/bsd/drm/kernel/drm/sysctl.c,v 1.2 2001/03/02 02:45:38 dawes Exp $
 *
 */

#define __NO_VERSION__
#include "drmP.h"
#include <sys/sysctl.h>

SYSCTL_NODE(_hw, OID_AUTO, dri, CTLFLAG_RW, 0, "DRI Graphics");

static int	   drm_name_info DRM_SYSCTL_HANDLER_ARGS;
static int	   drm_vm_info DRM_SYSCTL_HANDLER_ARGS;
static int	   drm_clients_info DRM_SYSCTL_HANDLER_ARGS;
static int	   drm_queues_info DRM_SYSCTL_HANDLER_ARGS;
static int	   drm_bufs_info DRM_SYSCTL_HANDLER_ARGS;
#if DRM_DEBUG_CODExx
static int	   drm_vma_info DRM_SYSCTL_HANDLER_ARGS;
#endif
#if DRM_DMA_HISTOGRAM
static int	   drm_histo_info DRM_SYSCTL_HANDLER_ARGS;
#endif

struct drm_sysctl_list {
	const char *name;
	int	   (*f) DRM_SYSCTL_HANDLER_ARGS;
} drm_sysctl_list[] = {
	{ "name",    drm_name_info    },
	{ "mem",     drm_mem_info     },
	{ "vm",	     drm_vm_info      },
	{ "clients", drm_clients_info },
	{ "queues",  drm_queues_info  },
	{ "bufs",    drm_bufs_info    },
#if DRM_DEBUG_CODExx
	{ "vma",     drm_vma_info     },
#endif
#if DRM_DMA_HISTOGRAM
	{ "histo",   drm_histo_info   },
#endif
};
#define DRM_SYSCTL_ENTRIES (sizeof(drm_sysctl_list)/sizeof(drm_sysctl_list[0]))

struct drm_sysctl_info {
	struct sysctl_oid      oids[DRM_SYSCTL_ENTRIES + 1];
	struct sysctl_oid_list list;
	char		       name[2];
};

int drm_sysctl_init(drm_device_t *dev)
{
	struct drm_sysctl_info *info;
	struct sysctl_oid *oid;
	struct sysctl_oid *top;
	int		  i;

	/* Find the next free slot under hw.graphics */
	i = 0;
	SLIST_FOREACH(oid, &sysctl__hw_dri_children, oid_link) {
		if (i <= oid->oid_arg2)
			i = oid->oid_arg2 + 1;
	}
	
	info = drm_alloc(sizeof *info, DRM_MEM_DRIVER);
	dev->sysctl = info;

	/* Construct the node under hw.graphics */
	info->name[0] = '0' + i;
	info->name[1] = 0;
	oid = &info->oids[DRM_SYSCTL_ENTRIES];
	bzero(oid, sizeof(*oid));
	oid->oid_parent = &sysctl__hw_dri_children;
	oid->oid_number = OID_AUTO;
	oid->oid_kind = CTLTYPE_NODE | CTLFLAG_RW;
	oid->oid_arg1 = &info->list;
	oid->oid_arg2 = i;
	oid->oid_name = info->name;
	oid->oid_handler = 0;
	oid->oid_fmt = "N";
	SLIST_INIT(&info->list);
	sysctl_register_oid(oid);
	top = oid;

	for (i = 0; i < DRM_SYSCTL_ENTRIES; i++) {
		oid = &info->oids[i];
		bzero(oid, sizeof(*oid));
		oid->oid_parent = top->oid_arg1;
		oid->oid_number = OID_AUTO;
		oid->oid_kind = CTLTYPE_INT | CTLFLAG_RD;
		oid->oid_arg1 = dev;
		oid->oid_arg2 = 0;
		oid->oid_name = drm_sysctl_list[i].name;
		oid->oid_handler = drm_sysctl_list[i].f;
		oid->oid_fmt = "A";
		sysctl_register_oid(oid);
	}

	return 0;
}

int drm_sysctl_cleanup(drm_device_t *dev)
{
	int i;

	DRM_DEBUG("dev->sysctl=%p\n", dev->sysctl);
	for (i = 0; i < DRM_SYSCTL_ENTRIES + 1; i++)
		sysctl_unregister_oid(&dev->sysctl->oids[i]);

	drm_free(dev->sysctl, sizeof *dev->sysctl, DRM_MEM_DRIVER);
	dev->sysctl = NULL;

	return 0;
}

static int drm_name_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	char buf[128];
	int error;

	if (dev->unique) {
		DRM_SYSCTL_PRINT("%s 0x%x %s\n",
			       dev->name, dev2udev(dev->devnode), dev->unique);
	} else {
		DRM_SYSCTL_PRINT("%s 0x%x\n", dev->name, dev2udev(dev->devnode));
	}

	SYSCTL_OUT(req, "", 1);

	return 0;
}

static int _drm_vm_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	drm_map_t    *map;
	const char   *types[] = { "FB", "REG", "SHM", "AGP" };
	const char   *type;
	int	     i;
	char         buf[128];
	int          error;

	DRM_SYSCTL_PRINT("slot	 offset	      size type flags	 "
			 "address mtrr\n\n");
	error = SYSCTL_OUT(req, buf, strlen(buf));
	if (error) return error;

	for (i = 0; i < dev->map_count; i++) {
		map = dev->maplist[i];
		if (map->type < 0 || map->type > 3) type = "??";
		else				    type = types[map->type];
		DRM_SYSCTL_PRINT("%4d 0x%08lx 0x%08lx %4.4s  0x%02x 0x%08lx ",
				 i,
				 map->offset,
				 map->size,
				 type,
				 map->flags,
				 (unsigned long)map->handle);
		if (map->mtrr < 0) {
			DRM_SYSCTL_PRINT("none\n");
		} else {
			DRM_SYSCTL_PRINT("%4d\n", map->mtrr);
		}
	}
	SYSCTL_OUT(req, "", 1);

	return 0;
}

static int drm_vm_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	int	     ret;

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	ret = _drm_vm_info(oidp, arg1, arg2, req);
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);

	return ret;
}


static int _drm_queues_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	int	     i;
	drm_queue_t  *q;
	char         buf[128];
	int          error;

	DRM_SYSCTL_PRINT("  ctx/flags   use   fin"
			 "   blk/rw/rwf  wait    flushed	   queued"
			 "      locks\n\n");
	for (i = 0; i < dev->queue_count; i++) {
		q = dev->queuelist[i];
		atomic_inc(&q->use_count);
		DRM_SYSCTL_PRINT_RET(atomic_dec(&q->use_count),
				     "%5d/0x%03x %5d %5d"
				     " %5d/%c%c/%c%c%c %5Zd %10d %10d %10d\n",
				     i,
				     q->flags,
				     atomic_read(&q->use_count),
				     atomic_read(&q->finalization),
				     atomic_read(&q->block_count),
				     atomic_read(&q->block_read) ? 'r' : '-',
				     atomic_read(&q->block_write) ? 'w' : '-',
				     q->read_queue ? 'r':'-',
				     q->write_queue ? 'w':'-',
				     q->flush_queue ? 'f':'-',
				     DRM_BUFCOUNT(&q->waitlist),
				     atomic_read(&q->total_flushed),
				     atomic_read(&q->total_queued),
				     atomic_read(&q->total_locks));
		atomic_dec(&q->use_count);
	}

	SYSCTL_OUT(req, "", 1);
	return 0;
}

static int drm_queues_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	int	     ret;

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	ret = _drm_queues_info(oidp, arg1, arg2, req);
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
	return ret;
}

/* drm_bufs_info is called whenever a process reads
   hw.dri.0.bufs. */

static int _drm_bufs_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t	 *dev = arg1;
	drm_device_dma_t *dma = dev->dma;
	int		 i;
	char             buf[128];
	int              error;

	if (!dma)	return 0;
	DRM_SYSCTL_PRINT(" o     size count  free	 segs pages    kB\n\n");
	for (i = 0; i <= DRM_MAX_ORDER; i++) {
		if (dma->bufs[i].buf_count)
			DRM_SYSCTL_PRINT("%2d %8d %5d %5d %5d %5d %5d\n",
				       i,
				       dma->bufs[i].buf_size,
				       dma->bufs[i].buf_count,
				       atomic_read(&dma->bufs[i]
						   .freelist.count),
				       dma->bufs[i].seg_count,
				       dma->bufs[i].seg_count
				       *(1 << dma->bufs[i].page_order),
				       (dma->bufs[i].seg_count
					* (1 << dma->bufs[i].page_order))
				       * PAGE_SIZE / 1024);
	}
	DRM_SYSCTL_PRINT("\n");
	for (i = 0; i < dma->buf_count; i++) {
		if (i && !(i%32)) DRM_SYSCTL_PRINT("\n");
		DRM_SYSCTL_PRINT(" %d", dma->buflist[i]->list);
	}
	DRM_SYSCTL_PRINT("\n");

	SYSCTL_OUT(req, "", 1);
	return 0;
}

static int drm_bufs_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	int	     ret;

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	ret = _drm_bufs_info(oidp, arg1, arg2, req);
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
	return ret;
}


static int _drm_clients_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	drm_file_t   *priv;
	char         buf[128];
	int          error;

	DRM_SYSCTL_PRINT("a dev	pid    uid	magic	  ioctls\n\n");
	TAILQ_FOREACH(priv, &dev->files, link) {
		DRM_SYSCTL_PRINT("%c %3d %5d %5d %10u %10lu\n",
			       priv->authenticated ? 'y' : 'n',
			       priv->minor,
			       priv->pid,
			       priv->uid,
			       priv->magic,
			       priv->ioctl_count);
	}

	SYSCTL_OUT(req, "", 1);
	return 0;
}

static int drm_clients_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	int	     ret;

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	ret = _drm_clients_info(oidp, arg1, arg2, req);
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
	return ret;
}

#if DRM_DEBUG_CODExx

static int _drm_vma_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t	      *dev = arg1;
	drm_vma_entry_t	      *pt;
	pgd_t		      *pgd;
	pmd_t		      *pmd;
	pte_t		      *pte;
	unsigned long	      i;
	struct vm_area_struct *vma;
	unsigned long	      address;
#if defined(__i386__)
	unsigned int	      pgprot;
#endif
	char		      buf[128];
	int		      error;

	DRM_SYSCTL_PRINT("vma use count: %d, high_memory = %p, 0x%08lx\n",
		       atomic_read(&dev->vma_count),
		       high_memory, virt_to_phys(high_memory));
	for (pt = dev->vmalist; pt; pt = pt->next) {
		if (!(vma = pt->vma)) continue;
		DRM_SYSCTL_PRINT("\n%5d 0x%08lx-0x%08lx %c%c%c%c%c%c 0x%08lx",
			       pt->pid,
			       vma->vm_start,
			       vma->vm_end,
			       vma->vm_flags & VM_READ	   ? 'r' : '-',
			       vma->vm_flags & VM_WRITE	   ? 'w' : '-',
			       vma->vm_flags & VM_EXEC	   ? 'x' : '-',
			       vma->vm_flags & VM_MAYSHARE ? 's' : 'p',
			       vma->vm_flags & VM_LOCKED   ? 'l' : '-',
			       vma->vm_flags & VM_IO	   ? 'i' : '-',
			       vma->vm_offset );
#if defined(__i386__)
		pgprot = pgprot_val(vma->vm_page_prot);
		DRM_SYSCTL_PRINT(" %c%c%c%c%c%c%c%c%c",
			       pgprot & _PAGE_PRESENT  ? 'p' : '-',
			       pgprot & _PAGE_RW       ? 'w' : 'r',
			       pgprot & _PAGE_USER     ? 'u' : 's',
			       pgprot & _PAGE_PWT      ? 't' : 'b',
			       pgprot & _PAGE_PCD      ? 'u' : 'c',
			       pgprot & _PAGE_ACCESSED ? 'a' : '-',
			       pgprot & _PAGE_DIRTY    ? 'd' : '-',
			       pgprot & _PAGE_4M       ? 'm' : 'k',
			       pgprot & _PAGE_GLOBAL   ? 'g' : 'l' );
#endif		
		DRM_SYSCTL_PRINT("\n");
		for (i = vma->vm_start; i < vma->vm_end; i += PAGE_SIZE) {
			pgd = pgd_offset(vma->vm_mm, i);
			pmd = pmd_offset(pgd, i);
			pte = pte_offset(pmd, i);
			if (pte_present(*pte)) {
				address = __pa(pte_page(*pte))
					+ (i & (PAGE_SIZE-1));
				DRM_SYSCTL_PRINT("      0x%08lx -> 0x%08lx"
					       " %c%c%c%c%c\n",
					       i,
					       address,
					       pte_read(*pte)  ? 'r' : '-',
					       pte_write(*pte) ? 'w' : '-',
					       pte_exec(*pte)  ? 'x' : '-',
					       pte_dirty(*pte) ? 'd' : '-',
					       pte_young(*pte) ? 'a' : '-' );
			} else {
				DRM_SYSCTL_PRINT("      0x%08lx\n", i);
			}
		}
	}
	
	SYSCTL_OUT(req, "", 1);
	return 0;
}

static int drm_vma_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	int	     ret;

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	ret = _drm_vma_info(oidp, arg1, arg2, req);
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
	return ret;
}
#endif


#if DRM_DMA_HISTOGRAM
static int _drm_histo_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t	 *dev = arg1;
	drm_device_dma_t *dma = dev->dma;
	int		 i;
	unsigned long	 slot_value = DRM_DMA_HISTOGRAM_INITIAL;
	unsigned long	 prev_value = 0;
	drm_buf_t	 *buffer;
	char		 buf[128];
	int              error;

	DRM_SYSCTL_PRINT("general statistics:\n");
	DRM_SYSCTL_PRINT("total	 %10u\n", atomic_read(&dev->histo.total));
	DRM_SYSCTL_PRINT("open	 %10u\n", atomic_read(&dev->total_open));
	DRM_SYSCTL_PRINT("close	 %10u\n", atomic_read(&dev->total_close));
	DRM_SYSCTL_PRINT("ioctl	 %10u\n", atomic_read(&dev->total_ioctl));
	DRM_SYSCTL_PRINT("irq	 %10u\n", atomic_read(&dev->total_irq));
	DRM_SYSCTL_PRINT("ctx	 %10u\n", atomic_read(&dev->total_ctx));
	
	DRM_SYSCTL_PRINT("\nlock statistics:\n");
	DRM_SYSCTL_PRINT("locks	 %10u\n", atomic_read(&dev->total_locks));
	DRM_SYSCTL_PRINT("unlocks	 %10u\n", atomic_read(&dev->total_unlocks));
	DRM_SYSCTL_PRINT("contends %10u\n", atomic_read(&dev->total_contends));
	DRM_SYSCTL_PRINT("sleeps	 %10u\n", atomic_read(&dev->total_sleeps));


	if (dma) {
		DRM_SYSCTL_PRINT("\ndma statistics:\n");
		DRM_SYSCTL_PRINT("prio	 %10u\n",
			       atomic_read(&dma->total_prio));
		DRM_SYSCTL_PRINT("bytes	 %10u\n",
			       atomic_read(&dma->total_bytes));
		DRM_SYSCTL_PRINT("dmas	 %10u\n",
			       atomic_read(&dma->total_dmas));
		DRM_SYSCTL_PRINT("missed:\n");
		DRM_SYSCTL_PRINT("  dma	 %10u\n",
			       atomic_read(&dma->total_missed_dma));
		DRM_SYSCTL_PRINT("  lock	 %10u\n",
			       atomic_read(&dma->total_missed_lock));
		DRM_SYSCTL_PRINT("  free	 %10u\n",
			       atomic_read(&dma->total_missed_free));
		DRM_SYSCTL_PRINT("  sched	 %10u\n",
			       atomic_read(&dma->total_missed_sched));
		DRM_SYSCTL_PRINT("tried	 %10u\n",
			       atomic_read(&dma->total_tried));
		DRM_SYSCTL_PRINT("hit	 %10u\n",
			       atomic_read(&dma->total_hit));
		DRM_SYSCTL_PRINT("lost	 %10u\n",
			       atomic_read(&dma->total_lost));
		
		buffer = dma->next_buffer;
		if (buffer) {
			DRM_SYSCTL_PRINT("next_buffer %7d\n", buffer->idx);
		} else {
			DRM_SYSCTL_PRINT("next_buffer    none\n");
		}
		buffer = dma->this_buffer;
		if (buffer) {
			DRM_SYSCTL_PRINT("this_buffer %7d\n", buffer->idx);
		} else {
			DRM_SYSCTL_PRINT("this_buffer    none\n");
		}
	}
	

	DRM_SYSCTL_PRINT("\nvalues:\n");
	if (dev->lock.hw_lock) {
		DRM_SYSCTL_PRINT("lock	       0x%08x\n",
			       dev->lock.hw_lock->lock);
	} else {
		DRM_SYSCTL_PRINT("lock		     none\n");
	}
	DRM_SYSCTL_PRINT("context_flag   0x%08lx\n", dev->context_flag);
	DRM_SYSCTL_PRINT("interrupt_flag 0x%08lx\n", dev->interrupt_flag);
	DRM_SYSCTL_PRINT("dma_flag       0x%08lx\n", dev->dma_flag);

	DRM_SYSCTL_PRINT("queue_count    %10d\n",	 dev->queue_count);
	DRM_SYSCTL_PRINT("last_context   %10d\n",	 dev->last_context);
	DRM_SYSCTL_PRINT("last_switch    %10u\n",	 dev->last_switch);
	DRM_SYSCTL_PRINT("last_checked   %10d\n",	 dev->last_checked);
		
	
	DRM_SYSCTL_PRINT("\n		       q2d	  d2c	     c2f"
		       "	q2c	   q2f	      dma	 sch"
		       "	ctx	  lacq	     lhld\n\n");
	for (i = 0; i < DRM_DMA_HISTOGRAM_SLOTS; i++) {
		DRM_SYSCTL_PRINT("%s %10lu %10u %10u %10u %10u %10u"
			       " %10u %10u %10u %10u %10u\n",
			       i == DRM_DMA_HISTOGRAM_SLOTS - 1 ? ">=" : "< ",
			       i == DRM_DMA_HISTOGRAM_SLOTS - 1
			       ? prev_value : slot_value ,
			       
			       atomic_read(&dev->histo
					   .queued_to_dispatched[i]),
			       atomic_read(&dev->histo
					   .dispatched_to_completed[i]),
			       atomic_read(&dev->histo
					   .completed_to_freed[i]),
			       
			       atomic_read(&dev->histo
					   .queued_to_completed[i]),
			       atomic_read(&dev->histo
					   .queued_to_freed[i]),
			       atomic_read(&dev->histo.dma[i]),
			       atomic_read(&dev->histo.schedule[i]),
			       atomic_read(&dev->histo.ctx[i]),
			       atomic_read(&dev->histo.lacq[i]),
			       atomic_read(&dev->histo.lhld[i]));
		prev_value = slot_value;
		slot_value = DRM_DMA_HISTOGRAM_NEXT(slot_value);
	}
	SYSCTL_OUT(req, "", 1);
	return 0;
}

static int drm_histo_info DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	int	     ret;

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	ret = _drm_histo_info(oidp, arg1, arg2, req);
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
	return ret;
}
#endif

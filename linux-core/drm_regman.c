/**************************************************************************
 * Copyright (c) 2007 Tungsten Graphics, Inc., Cedar Park, TX., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * An allocate-fence manager implementation intended for sets of base-registers
 * or tiling-registers.
 */

#include "drmP.h"

/*
 * Allocate a compatible register and put it on the unfenced list.
 */

int drm_regs_alloc(struct drm_reg_manager *manager,
		   const void *data,
		   uint32_t fence_class,
		   uint32_t fence_type,
		   int interruptible, int no_wait, struct drm_reg **reg)
{
	struct drm_reg *entry, *next_entry;
	int ret;

	*reg = NULL;

	/*
	 * Search the unfenced list.
	 */

	list_for_each_entry(entry, &manager->unfenced, head) {
		if (manager->reg_reusable(entry, data)) {
			entry->new_fence_type |= fence_type;
			goto out;
		}
	}

	/*
	 * Search the lru list.
	 */

	list_for_each_entry_safe(entry, next_entry, &manager->lru, head) {
		struct drm_fence_object *fence = entry->fence;
		if (fence->fence_class == fence_class &&
		    (entry->fence_type & fence_type) == entry->fence_type &&
		    manager->reg_reusable(entry, data)) {
			list_del(&entry->head);
			entry->new_fence_type = fence_type;
			list_add_tail(&entry->head, &manager->unfenced);
			goto out;
		}
	}

	/*
	 * Search the free list.
	 */

	list_for_each_entry(entry, &manager->free, head) {
		list_del(&entry->head);
		entry->new_fence_type = fence_type;
		list_add_tail(&entry->head, &manager->unfenced);
		goto out;
	}

	if (no_wait)
		return -EBUSY;

	/*
	 * Go back to the lru list and try to expire fences.
	 */

	list_for_each_entry_safe(entry, next_entry, &manager->lru, head) {
		BUG_ON(!entry->fence);
		ret = drm_fence_object_wait(entry->fence, 0, !interruptible,
					    entry->fence_type);
		if (ret)
			return ret;

		drm_fence_usage_deref_unlocked(&entry->fence);
		list_del(&entry->head);
		entry->new_fence_type = fence_type;
		list_add_tail(&entry->head, &manager->unfenced);
		goto out;
	}

	/*
	 * Oops. All registers are used up :(.
	 */

	return -EBUSY;
      out:
	*reg = entry;
	return 0;
}

EXPORT_SYMBOL(drm_regs_alloc);

void drm_regs_fence(struct drm_reg_manager *manager,
		    struct drm_fence_object *fence)
{
	struct drm_reg *entry;
	struct drm_reg *next_entry;

	if (!fence) {

		/*
		 * Old fence (if any) is still valid.
		 * Put back on free and lru lists.
		 */

		list_for_each_entry_safe_reverse(entry, next_entry,
						 &manager->unfenced, head) {
			list_del(&entry->head);
			list_add(&entry->head, (entry->fence) ?
				 &manager->lru : &manager->free);
		}
	} else {

		/*
		 * Fence with a new fence and put on lru list.
		 */

		list_for_each_entry_safe(entry, next_entry, &manager->unfenced,
					 head) {
			list_del(&entry->head);
			if (entry->fence)
				drm_fence_usage_deref_unlocked(&entry->fence);
			drm_fence_reference_unlocked(&entry->fence, fence);

			entry->fence_type = entry->new_fence_type;
			BUG_ON((entry->fence_type & fence->type) !=
			       entry->fence_type);

			list_add_tail(&entry->head, &manager->lru);
		}
	}
}

EXPORT_SYMBOL(drm_regs_fence);

void drm_regs_free(struct drm_reg_manager *manager)
{
	struct drm_reg *entry;
	struct drm_reg *next_entry;

	drm_regs_fence(manager, NULL);

	list_for_each_entry_safe(entry, next_entry, &manager->free, head) {
		list_del(&entry->head);
		manager->reg_destroy(entry);
	}

	list_for_each_entry_safe(entry, next_entry, &manager->lru, head) {

		(void)drm_fence_object_wait(entry->fence, 1, 1,
					    entry->fence_type);
		list_del(&entry->head);
		drm_fence_usage_deref_unlocked(&entry->fence);
		manager->reg_destroy(entry);
	}
}

EXPORT_SYMBOL(drm_regs_free);

void drm_regs_add(struct drm_reg_manager *manager, struct drm_reg *reg)
{
	reg->fence = NULL;
	list_add_tail(&reg->head, &manager->free);
}

EXPORT_SYMBOL(drm_regs_add);

void drm_regs_init(struct drm_reg_manager *manager,
		   int (*reg_reusable) (const struct drm_reg *, const void *),
		   void (*reg_destroy) (struct drm_reg *))
{
	INIT_LIST_HEAD(&manager->free);
	INIT_LIST_HEAD(&manager->lru);
	INIT_LIST_HEAD(&manager->unfenced);
	manager->reg_reusable = reg_reusable;
	manager->reg_destroy = reg_destroy;
}

EXPORT_SYMBOL(drm_regs_init);

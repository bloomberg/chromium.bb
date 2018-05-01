// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_BUCKET_INL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_BUCKET_INL_H_

#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/compiler_specific.h"
#include "base/logging.h"

namespace base {
namespace internal {

// TODO(ajwong): Move this to PartitionRootBase. Likely can remove this -inl.h
// file.
ALWAYS_INLINE void* PartitionBucket::Alloc(PartitionRootBase* root,
                                           int flags,
                                           size_t size) {
  PartitionPage* page = this->active_pages_head;
  // Check that this page is neither full nor freed.
  DCHECK(page->num_allocated_slots >= 0);
  void* ret = page->freelist_head;
  if (LIKELY(ret != 0)) {
    // If these DCHECKs fire, you probably corrupted memory.
    // TODO(palmer): See if we can afford to make this a CHECK.
    DCHECK(PartitionPage::IsPointerValid(page));
    // All large allocations must go through the slow path to correctly
    // update the size metadata.
    DCHECK(page->get_raw_size() == 0);
    internal::PartitionFreelistEntry* new_head =
        internal::PartitionFreelistEntry::Transform(
            static_cast<internal::PartitionFreelistEntry*>(ret)->next);
    page->freelist_head = new_head;
    page->num_allocated_slots++;
  } else {
    ret = this->SlowPathAlloc(root, flags, size);
    // TODO(palmer): See if we can afford to make this a CHECK.
    DCHECK(!ret ||
           PartitionPage::IsPointerValid(PartitionPage::FromPointer(ret)));
  }
#if DCHECK_IS_ON()
  if (!ret)
    return 0;
  // Fill the uninitialized pattern, and write the cookies.
  page = PartitionPage::FromPointer(ret);
  // TODO(ajwong): Can |page->bucket| ever not be |this|? If not, can this just
  // be this->slot_size?
  size_t new_slot_size = page->bucket->slot_size;
  size_t raw_size = page->get_raw_size();
  if (raw_size) {
    DCHECK(raw_size == size);
    new_slot_size = raw_size;
  }
  size_t no_cookie_size = PartitionCookieSizeAdjustSubtract(new_slot_size);
  char* char_ret = static_cast<char*>(ret);
  // The value given to the application is actually just after the cookie.
  ret = char_ret + kCookieSize;

  // Debug fill region kUninitializedByte and surround it with 2 cookies.
  PartitionCookieWriteValue(char_ret);
  memset(ret, kUninitializedByte, no_cookie_size);
  PartitionCookieWriteValue(char_ret + kCookieSize + no_cookie_size);
#endif
  return ret;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_BUCKET_INL_H_

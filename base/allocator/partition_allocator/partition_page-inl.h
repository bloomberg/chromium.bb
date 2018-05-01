// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_INL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_INL_H_

#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_root_base.h"

namespace base {
namespace internal {

ALWAYS_INLINE char* PartitionSuperPageToMetadataArea(char* ptr) {
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(ptr);
  DCHECK(!(pointer_as_uint & kSuperPageOffsetMask));
  // The metadata area is exactly one system page (the guard page) into the
  // super page.
  return reinterpret_cast<char*>(pointer_as_uint + kSystemPageSize);
}

ALWAYS_INLINE PartitionPage* PartitionPage::FromPointerNoAlignmentCheck(
    void* ptr) {
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(ptr);
  char* super_page_ptr =
      reinterpret_cast<char*>(pointer_as_uint & kSuperPageBaseMask);
  uintptr_t partition_page_index =
      (pointer_as_uint & kSuperPageOffsetMask) >> kPartitionPageShift;
  // Index 0 is invalid because it is the metadata and guard area and
  // the last index is invalid because it is a guard page.
  DCHECK(partition_page_index);
  DCHECK(partition_page_index < kNumPartitionPagesPerSuperPage - 1);
  PartitionPage* page = reinterpret_cast<PartitionPage*>(
      PartitionSuperPageToMetadataArea(super_page_ptr) +
      (partition_page_index << kPageMetadataShift));
  // Partition pages in the same slot span can share the same page object.
  // Adjust for that.
  size_t delta = page->page_offset << kPageMetadataShift;
  page =
      reinterpret_cast<PartitionPage*>(reinterpret_cast<char*>(page) - delta);
  return page;
}

// Resturns start of the slot span for the PartitionPage.
ALWAYS_INLINE void* PartitionPage::ToPointer(const PartitionPage* page) {
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(page);

  uintptr_t super_page_offset = (pointer_as_uint & kSuperPageOffsetMask);

  // A valid |page| must be past the first guard System page and within
  // the following metadata region.
  DCHECK(super_page_offset > kSystemPageSize);
  // Must be less than total metadata region.
  DCHECK(super_page_offset < kSystemPageSize + (kNumPartitionPagesPerSuperPage *
                                                kPageMetadataSize));
  uintptr_t partition_page_index =
      (super_page_offset - kSystemPageSize) >> kPageMetadataShift;
  // Index 0 is invalid because it is the superpage extent metadata and the
  // last index is invalid because the whole PartitionPage is set as guard
  // pages for the metadata region.
  DCHECK(partition_page_index);
  DCHECK(partition_page_index < kNumPartitionPagesPerSuperPage - 1);
  uintptr_t super_page_base = (pointer_as_uint & kSuperPageBaseMask);
  void* ret = reinterpret_cast<void*>(
      super_page_base + (partition_page_index << kPartitionPageShift));
  return ret;
}

ALWAYS_INLINE PartitionPage* PartitionPage::FromPointer(void* ptr) {
  PartitionPage* page = PartitionPage::FromPointerNoAlignmentCheck(ptr);
  // Checks that the pointer is a multiple of bucket size.
  DCHECK(!((reinterpret_cast<uintptr_t>(ptr) -
            reinterpret_cast<uintptr_t>(PartitionPage::ToPointer(page))) %
           page->bucket->slot_size));
  return page;
}

ALWAYS_INLINE const size_t* PartitionPage::get_raw_size_ptr() const {
  // For single-slot buckets which span more than one partition page, we
  // have some spare metadata space to store the raw allocation size. We
  // can use this to report better statistics.
  if (bucket->slot_size <= kMaxSystemPagesPerSlotSpan * kSystemPageSize)
    return nullptr;

  DCHECK((bucket->slot_size % kSystemPageSize) == 0);
  DCHECK(bucket->is_direct_mapped() || bucket->get_slots_per_span() == 1);

  const PartitionPage* the_next_page = this + 1;
  return reinterpret_cast<const size_t*>(&the_next_page->freelist_head);
}

ALWAYS_INLINE size_t PartitionPage::get_raw_size() const {
  const size_t* ptr = get_raw_size_ptr();
  if (UNLIKELY(ptr != nullptr))
    return *ptr;
  return 0;
}

ALWAYS_INLINE bool PartitionPage::IsPointerValid(PartitionPage* page) {
  PartitionRootBase* root = PartitionRootBase::FromPage(page);
  return root->inverted_self == ~reinterpret_cast<uintptr_t>(root);
}

ALWAYS_INLINE void PartitionPage::Free(void* ptr) {
// If these asserts fire, you probably corrupted memory.
#if DCHECK_IS_ON()
  size_t slot_size = this->bucket->slot_size;
  size_t raw_size = get_raw_size();
  if (raw_size)
    slot_size = raw_size;
  PartitionCookieCheckValue(ptr);
  PartitionCookieCheckValue(reinterpret_cast<char*>(ptr) + slot_size -
                            kCookieSize);
  memset(ptr, kFreedByte, slot_size);
#endif
  DCHECK(this->num_allocated_slots);
  // TODO(palmer): See if we can afford to make this a CHECK.
  DCHECK(!freelist_head || PartitionPage::IsPointerValid(
                               PartitionPage::FromPointer(freelist_head)));
  CHECK(ptr != freelist_head);  // Catches an immediate double free.
  // Look for double free one level deeper in debug.
  DCHECK(!freelist_head || ptr != internal::PartitionFreelistEntry::Transform(
                                      freelist_head->next));
  internal::PartitionFreelistEntry* entry =
      static_cast<internal::PartitionFreelistEntry*>(ptr);
  entry->next = internal::PartitionFreelistEntry::Transform(freelist_head);
  freelist_head = entry;
  --this->num_allocated_slots;
  if (UNLIKELY(this->num_allocated_slots <= 0)) {
    FreeSlowPath();
  } else {
    // All single-slot allocations must go through the slow path to
    // correctly update the size metadata.
    DCHECK(get_raw_size() == 0);
  }
}

ALWAYS_INLINE bool PartitionPage::is_active() const {
  DCHECK(this != get_sentinel_page());
  DCHECK(!page_offset);
  return (num_allocated_slots > 0 &&
          (freelist_head || num_unprovisioned_slots));
}

ALWAYS_INLINE bool PartitionPage::is_full() const {
  DCHECK(this != get_sentinel_page());
  DCHECK(!page_offset);
  bool ret = (num_allocated_slots == bucket->get_slots_per_span());
  if (ret) {
    DCHECK(!freelist_head);
    DCHECK(!num_unprovisioned_slots);
  }
  return ret;
}

ALWAYS_INLINE bool PartitionPage::is_empty() const {
  DCHECK(this != get_sentinel_page());
  DCHECK(!page_offset);
  return (!num_allocated_slots && freelist_head);
}

ALWAYS_INLINE bool PartitionPage::is_decommitted() const {
  DCHECK(this != get_sentinel_page());
  DCHECK(!page_offset);
  bool ret = (!num_allocated_slots && !freelist_head);
  if (ret) {
    DCHECK(!num_unprovisioned_slots);
    DCHECK(empty_cache_index == -1);
  }
  return ret;
}

ALWAYS_INLINE void PartitionPage::set_raw_size(size_t size) {
  size_t* raw_size_ptr = get_raw_size_ptr();
  if (UNLIKELY(raw_size_ptr != nullptr))
    *raw_size_ptr = size;
}

ALWAYS_INLINE void PartitionPage::Reset() {
  DCHECK(this->is_decommitted());

  num_unprovisioned_slots = bucket->get_slots_per_span();
  DCHECK(num_unprovisioned_slots);

  next_page = nullptr;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_INL_H_

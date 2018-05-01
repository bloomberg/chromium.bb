// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_

#include "base/allocator/partition_allocator/partition_alloc_constants.h"

namespace base {
namespace internal {

struct PartitionBucket;
struct PartitionFreelistEntry;
struct PartitionRootBase;

// Some notes on page states. A page can be in one of four major states:
// 1) Active.
// 2) Full.
// 3) Empty.
// 4) Decommitted.
// An active page has available free slots. A full page has no free slots. An
// empty page has no free slots, and a decommitted page is an empty page that
// had its backing memory released back to the system.
// There are two linked lists tracking the pages. The "active page" list is an
// approximation of a list of active pages. It is an approximation because
// full, empty and decommitted pages may briefly be present in the list until
// we next do a scan over it.
// The "empty page" list is an accurate list of pages which are either empty
// or decommitted.
//
// The significant page transitions are:
// - free() will detect when a full page has a slot free()'d and immediately
// return the page to the head of the active list.
// - free() will detect when a page is fully emptied. It _may_ add it to the
// empty list or it _may_ leave it on the active list until a future list scan.
// - malloc() _may_ scan the active page list in order to fulfil the request.
// If it does this, full, empty and decommitted pages encountered will be
// booted out of the active list. If there are no suitable active pages found,
// an empty or decommitted page (if one exists) will be pulled from the empty
// list on to the active list.
//
// TODO(ajwong): Evaluate if this should be named PartitionSlotSpanMetadata or
// similar. If so, all uses of the term "page" in comments, member variables,
// local variables, and documentation that refer to this concept should be
// updated.
struct PartitionPage {
  PartitionFreelistEntry* freelist_head;
  PartitionPage* next_page;
  PartitionBucket* bucket;
  // Deliberately signed, 0 for empty or decommitted page, -n for full pages:
  int16_t num_allocated_slots;
  uint16_t num_unprovisioned_slots;
  uint16_t page_offset;
  int16_t empty_cache_index;  // -1 if not in the empty cache.

  // Public API

  // Note the matching Alloc() functions are in PartitionPage.
  BASE_EXPORT NOINLINE void FreeSlowPath();
  ALWAYS_INLINE void Free(void* ptr);

  void Decommit(PartitionRootBase* root);
  void DecommitIfPossible(PartitionRootBase* root);

  // Pointer manipulation functions. These must be static as the input |page|
  // pointer may be the result of an offset calculation and therefore cannot
  // be trusted. The objective of these functions is to sanitize this input.
  ALWAYS_INLINE static void* ToPointer(const PartitionPage* page);
  ALWAYS_INLINE static PartitionPage* FromPointerNoAlignmentCheck(void* ptr);
  ALWAYS_INLINE static PartitionPage* FromPointer(void* ptr);
  ALWAYS_INLINE static bool IsPointerValid(PartitionPage* page);

  ALWAYS_INLINE const size_t* get_raw_size_ptr() const;
  ALWAYS_INLINE size_t* get_raw_size_ptr() {
    return const_cast<size_t*>(
        const_cast<const PartitionPage*>(this)->get_raw_size_ptr());
  }

  ALWAYS_INLINE size_t get_raw_size() const;
  ALWAYS_INLINE void set_raw_size(size_t size);

  ALWAYS_INLINE void Reset();

  // TODO(ajwong): Can this be made private?  https://crbug.com/787153
  BASE_EXPORT static PartitionPage* get_sentinel_page();

  // Page State accessors.
  // Note that it's only valid to call these functions on pages found on one of
  // the page lists. Specifically, you can't call these functions on full pages
  // that were detached from the active list.
  //
  // This restriction provides the flexibity for some of the status fields to
  // be repurposed when a page is taken off a list. See the negation of
  // |num_allocated_slots| when a full page is removed from the active list
  // for an example of such repurposing.
  ALWAYS_INLINE bool is_active() const;
  ALWAYS_INLINE bool is_full() const;
  ALWAYS_INLINE bool is_empty() const;
  ALWAYS_INLINE bool is_decommitted() const;

 private:
  // g_sentinel_page is used as a sentinel to indicate that there is no page
  // in the active page list. We can use nullptr, but in that case we need
  // to add a null-check branch to the hot allocation path. We want to avoid
  // that.
  //
  // Note, this declaration is kept in the header as opposed to an anonymous
  // namespace so the getter can be fully inlined.
  static PartitionPage sentinel_page_;
};
static_assert(sizeof(PartitionPage) <= kPageMetadataSize,
              "PartitionPage must be able to fit in a metadata slot");

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_

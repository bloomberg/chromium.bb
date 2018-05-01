// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_BASE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_BASE_H_

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"

namespace base {
namespace internal {

struct PartitionPage;
struct PartitionRootBase;

// An "extent" is a span of consecutive superpages. We link to the partition's
// next extent (if there is one) to the very start of a superpage's metadata
// area.
struct PartitionSuperPageExtentEntry {
  PartitionRootBase* root;
  char* super_page_base;
  char* super_pages_end;
  PartitionSuperPageExtentEntry* next;
};
static_assert(
    sizeof(PartitionSuperPageExtentEntry) <= kPageMetadataSize,
    "PartitionSuperPageExtentEntry must be able to fit in a metadata slot");

struct BASE_EXPORT PartitionRootBase {
  PartitionRootBase();
  virtual ~PartitionRootBase();
  size_t total_size_of_committed_pages = 0;
  size_t total_size_of_super_pages = 0;
  size_t total_size_of_direct_mapped_pages = 0;
  // Invariant: total_size_of_committed_pages <=
  //                total_size_of_super_pages +
  //                total_size_of_direct_mapped_pages.
  unsigned num_buckets = 0;
  unsigned max_allocation = 0;
  bool initialized = false;
  char* next_super_page = nullptr;
  char* next_partition_page = nullptr;
  char* next_partition_page_end = nullptr;
  PartitionSuperPageExtentEntry* current_extent = nullptr;
  PartitionSuperPageExtentEntry* first_extent = nullptr;
  PartitionDirectMapExtent* direct_map_list = nullptr;
  PartitionPage* global_empty_page_ring[kMaxFreeableSpans] = {};
  int16_t global_empty_page_ring_index = 0;
  uintptr_t inverted_self = 0;

  // Public API

  // gOomHandlingFunction is invoked when PartitionAlloc hits OutOfMemory.
  static void (*gOomHandlingFunction)();
  NOINLINE void OutOfMemory();

  ALWAYS_INLINE static PartitionRootBase* FromPage(PartitionPage* page);

  ALWAYS_INLINE void IncreaseCommittedPages(size_t len);
  ALWAYS_INLINE void DecreaseCommittedPages(size_t len);
  ALWAYS_INLINE void DecommitSystemPages(void* address, size_t length);
  ALWAYS_INLINE void RecommitSystemPages(void* address, size_t length);

  void DecommitEmptyPages();
};

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_BASE_H_

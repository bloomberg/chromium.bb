// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_BASE_INL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_BASE_INL_H_

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_root_base.h"

namespace base {
namespace internal {

ALWAYS_INLINE PartitionRootBase* PartitionRootBase::FromPage(
    PartitionPage* page) {
  PartitionSuperPageExtentEntry* extent_entry =
      reinterpret_cast<PartitionSuperPageExtentEntry*>(
          reinterpret_cast<uintptr_t>(page) & kSystemPageBaseMask);
  return extent_entry->root;
}

ALWAYS_INLINE void PartitionRootBase::IncreaseCommittedPages(size_t len) {
  total_size_of_committed_pages += len;
  DCHECK(total_size_of_committed_pages <=
         total_size_of_super_pages + total_size_of_direct_mapped_pages);
}

ALWAYS_INLINE void PartitionRootBase::DecreaseCommittedPages(size_t len) {
  total_size_of_committed_pages -= len;
  DCHECK(total_size_of_committed_pages <=
         total_size_of_super_pages + total_size_of_direct_mapped_pages);
}

ALWAYS_INLINE void PartitionRootBase::DecommitSystemPages(void* address,
                                                          size_t length) {
  ::base::DecommitSystemPages(address, length);
  DecreaseCommittedPages(length);
}

ALWAYS_INLINE void PartitionRootBase::RecommitSystemPages(void* address,
                                                          size_t length) {
  CHECK(::base::RecommitSystemPages(address, length, PageReadWrite));
  IncreaseCommittedPages(length);
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_BASE_INL_H_

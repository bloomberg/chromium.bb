// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_INL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_INL_H_

#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_page.h"

namespace base {
namespace internal {

ALWAYS_INLINE PartitionDirectMapExtent* PartitionDirectMapExtent::FromPage(
    PartitionPage* page) {
  DCHECK(page->bucket->is_direct_mapped());
  return reinterpret_cast<PartitionDirectMapExtent*>(
      reinterpret_cast<char*>(page) + 3 * kPageMetadataSize);
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_INL_H_

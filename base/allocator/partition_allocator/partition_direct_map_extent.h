// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_H_

namespace base {
namespace internal {

struct PartitionBucket;
struct PartitionPage;

struct PartitionDirectMapExtent {
  PartitionDirectMapExtent* next_extent;
  PartitionDirectMapExtent* prev_extent;
  PartitionBucket* bucket;
  size_t map_size;  // Mapped size, not including guard pages and meta-data.

  ALWAYS_INLINE static PartitionDirectMapExtent* FromPage(PartitionPage* page);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/discardable_shared_memory_heap.h"

#include "base/memory/discardable_shared_memory.h"
#include "base/process/process_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class DiscardableSharedMemoryHeapTest : public testing::Test {};

TEST_F(DiscardableSharedMemoryHeapTest, Basic) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  // Free list is initially empty.
  EXPECT_FALSE(heap.SearchFreeList(1));

  const size_t kBlocks = 10;
  size_t memory_size = block_size * kBlocks;

  scoped_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));

  // Create new span for memory.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap.Grow(memory.Pass(), memory_size));

  // Free list should still be empty as |new_span| is currently in use.
  EXPECT_FALSE(heap.SearchFreeList(1));

  // Done using |new_span|. Merge it into the free list.
  heap.MergeIntoFreeList(new_span.Pass());

  // Free list should not contain a span that is larger than kBlocks.
  EXPECT_FALSE(heap.SearchFreeList(kBlocks + 1));

  // Free list should contain a span that satisfies the request for kBlocks.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.SearchFreeList(kBlocks);
  ASSERT_TRUE(span);

  // Delete span and shared memory backing.
  heap.DeleteSpan(span.Pass());

  // Free list should be empty again.
  EXPECT_FALSE(heap.SearchFreeList(1));
}

TEST_F(DiscardableSharedMemoryHeapTest, SplitAndMerge) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  const size_t kBlocks = 6;
  size_t memory_size = block_size * kBlocks;

  scoped_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));
  scoped_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap.Grow(memory.Pass(), memory_size));

  // Split span into two.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> leftover =
      heap.Split(new_span.get(), 3);
  ASSERT_TRUE(leftover);

  // Merge |leftover| into free list.
  heap.MergeIntoFreeList(leftover.Pass());

  // Some of the memory is still in use.
  EXPECT_FALSE(heap.SearchFreeList(kBlocks));

  // Merge |span| into free list.
  heap.MergeIntoFreeList(new_span.Pass());

  // Remove a 2 page span from free list.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span1 = heap.SearchFreeList(2);
  ASSERT_TRUE(span1);

  // Remove another 2 page span from free list.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span2 = heap.SearchFreeList(2);
  ASSERT_TRUE(span2);

  // Merge |span1| back into free list.
  heap.MergeIntoFreeList(span1.Pass());

  // Some of the memory is still in use.
  EXPECT_FALSE(heap.SearchFreeList(kBlocks));

  // Merge |span2| back into free list.
  heap.MergeIntoFreeList(span2.Pass());

  // All memory has been returned to the free list.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> large_span =
      heap.SearchFreeList(kBlocks);
  EXPECT_TRUE(large_span);
}

TEST_F(DiscardableSharedMemoryHeapTest, Grow) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  scoped_ptr<base::DiscardableSharedMemory> memory1(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory1->CreateAndMap(block_size));
  heap.MergeIntoFreeList(heap.Grow(memory1.Pass(), block_size).Pass());

  // Remove a span from free list.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span1 = heap.SearchFreeList(1);
  EXPECT_TRUE(span1);

  // No more memory available.
  EXPECT_FALSE(heap.SearchFreeList(1));

  // Grow free list using new memory.
  scoped_ptr<base::DiscardableSharedMemory> memory2(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory2->CreateAndMap(block_size));
  heap.MergeIntoFreeList(heap.Grow(memory2.Pass(), block_size).Pass());

  // Memory should now be available.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span2 = heap.SearchFreeList(1);
  EXPECT_TRUE(span2);
}

}  // namespace
}  // namespace content

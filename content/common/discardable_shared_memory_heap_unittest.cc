// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/discardable_shared_memory_heap.h"

#include "base/memory/discardable_shared_memory.h"
#include "base/process/process_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(DiscardableSharedMemoryHeapTest, Basic) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  // Initial size should be 0.
  EXPECT_EQ(0u, heap.GetSize());

  // Initial free list size should be 0.
  EXPECT_EQ(0u, heap.GetFreeListSize());

  // Free list is initially empty.
  EXPECT_FALSE(heap.SearchFreeList(1, 0));

  const size_t kBlocks = 10;
  size_t memory_size = block_size * kBlocks;

  scoped_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));

  // Create new span for memory.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap.Grow(memory.Pass(), memory_size));

  // Size should match |memory_size|.
  EXPECT_EQ(memory_size, heap.GetSize());

  // Free list size should still be 0.
  EXPECT_EQ(0u, heap.GetFreeListSize());

  // Free list should still be empty as |new_span| is currently in use.
  EXPECT_FALSE(heap.SearchFreeList(1, 0));

  // Done using |new_span|. Merge it into the free list.
  heap.MergeIntoFreeList(new_span.Pass());

  // Free list size should now match |memory_size|.
  EXPECT_EQ(memory_size, heap.GetFreeListSize());

  // Free list should not contain a span that is larger than kBlocks.
  EXPECT_FALSE(heap.SearchFreeList(kBlocks + 1, 0));

  // Free list should contain a span that satisfies the request for kBlocks.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.SearchFreeList(kBlocks, 0);
  ASSERT_TRUE(span);

  // Free list should be empty again.
  EXPECT_FALSE(heap.SearchFreeList(1, 0));

  // Merge it into the free list again.
  heap.MergeIntoFreeList(span.Pass());
}

TEST(DiscardableSharedMemoryHeapTest, SplitAndMerge) {
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
  EXPECT_FALSE(heap.SearchFreeList(kBlocks, 0));

  // Merge |span| into free list.
  heap.MergeIntoFreeList(new_span.Pass());

  // Remove a 2 page span from free list.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span1 =
      heap.SearchFreeList(2, kBlocks);
  ASSERT_TRUE(span1);

  // Remove another 2 page span from free list.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span2 =
      heap.SearchFreeList(2, kBlocks);
  ASSERT_TRUE(span2);

  // Merge |span1| back into free list.
  heap.MergeIntoFreeList(span1.Pass());

  // Some of the memory is still in use.
  EXPECT_FALSE(heap.SearchFreeList(kBlocks, 0));

  // Merge |span2| back into free list.
  heap.MergeIntoFreeList(span2.Pass());

  // All memory has been returned to the free list.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> large_span =
      heap.SearchFreeList(kBlocks, 0);
  ASSERT_TRUE(large_span);

  // Merge it into the free list again.
  heap.MergeIntoFreeList(large_span.Pass());
}

TEST(DiscardableSharedMemoryHeapTest, MergeSingleBlockSpan) {
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
      heap.Split(new_span.get(), 5);
  ASSERT_TRUE(leftover);

  // Merge |new_span| into free list.
  heap.MergeIntoFreeList(new_span.Pass());

  // Merge |leftover| into free list.
  heap.MergeIntoFreeList(leftover.Pass());
}

TEST(DiscardableSharedMemoryHeapTest, Grow) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  scoped_ptr<base::DiscardableSharedMemory> memory1(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory1->CreateAndMap(block_size));
  heap.MergeIntoFreeList(heap.Grow(memory1.Pass(), block_size).Pass());

  // Remove a span from free list.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span1 =
      heap.SearchFreeList(1, 0);
  EXPECT_TRUE(span1);

  // No more memory available.
  EXPECT_FALSE(heap.SearchFreeList(1, 0));

  // Grow free list using new memory.
  scoped_ptr<base::DiscardableSharedMemory> memory2(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory2->CreateAndMap(block_size));
  heap.MergeIntoFreeList(heap.Grow(memory2.Pass(), block_size).Pass());

  // Memory should now be available.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span2 =
      heap.SearchFreeList(1, 0);
  EXPECT_TRUE(span2);

  // Merge spans into the free list again.
  heap.MergeIntoFreeList(span1.Pass());
  heap.MergeIntoFreeList(span2.Pass());
}

TEST(DiscardableSharedMemoryHeapTest, ReleaseFreeMemory) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  scoped_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.Grow(memory.Pass(), block_size);

  // Free list should be empty.
  EXPECT_EQ(0u, heap.GetFreeListSize());

  heap.ReleaseFreeMemory();

  // Size should still match |block_size|.
  EXPECT_EQ(block_size, heap.GetSize());

  heap.MergeIntoFreeList(span.Pass());
  heap.ReleaseFreeMemory();

  // Memory should have been released.
  EXPECT_EQ(0u, heap.GetSize());
  EXPECT_EQ(0u, heap.GetFreeListSize());
}

TEST(DiscardableSharedMemoryHeapTest, ReleasePurgedMemory) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  scoped_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.Grow(memory.Pass(), block_size);

  // Unlock memory so it can be purged.
  span->shared_memory()->Unlock(0, 0);

  // Purge and release shared memory.
  bool rv = span->shared_memory()->Purge(base::Time::Now());
  EXPECT_TRUE(rv);
  heap.ReleasePurgedMemory();

  // Shared memory backing for |span| should be gone.
  EXPECT_FALSE(span->shared_memory());

  // Size should be 0.
  EXPECT_EQ(0u, heap.GetSize());
}

TEST(DiscardableSharedMemoryHeapTest, Slack) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  const size_t kBlocks = 6;
  size_t memory_size = block_size * kBlocks;

  scoped_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));
  heap.MergeIntoFreeList(heap.Grow(memory.Pass(), memory_size).Pass());

  // No span in the free list that is less or equal to 3 + 1.
  EXPECT_FALSE(heap.SearchFreeList(3, 1));

  // No span in the free list that is less or equal to 3 + 2.
  EXPECT_FALSE(heap.SearchFreeList(3, 2));

  // No span in the free list that is less or equal to 1 + 4.
  EXPECT_FALSE(heap.SearchFreeList(1, 4));

  scoped_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.SearchFreeList(1, 5);
  EXPECT_TRUE(span);

  heap.MergeIntoFreeList(span.Pass());
}

}  // namespace
}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/common/discardable_shared_memory_heap.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/process/process_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace discardable_memory {
namespace {

void NullTask() {}

TEST(DiscardableSharedMemoryHeapTest, Basic) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  // Initial size should be 0.
  EXPECT_EQ(0u, heap.GetSize());

  // Initial size of free lists should be 0.
  EXPECT_EQ(0u, heap.GetSizeOfFreeLists());

  // Free lists are initially empty.
  EXPECT_FALSE(heap.SearchFreeLists(1, 0));

  const size_t kBlocks = 10;
  size_t memory_size = block_size * kBlocks;
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));

  // Create new span for memory.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap.Grow(std::move(memory), memory_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask)));

  // Size should match |memory_size|.
  EXPECT_EQ(memory_size, heap.GetSize());

  // Size of free lists should still be 0.
  EXPECT_EQ(0u, heap.GetSizeOfFreeLists());

  // Free list should still be empty as |new_span| is currently in use.
  EXPECT_FALSE(heap.SearchFreeLists(1, 0));

  // Done using |new_span|. Merge it into the free lists.
  heap.MergeIntoFreeLists(std::move(new_span));

  // Size of free lists should now match |memory_size|.
  EXPECT_EQ(memory_size, heap.GetSizeOfFreeLists());

  // Free lists should not contain a span that is larger than kBlocks.
  EXPECT_FALSE(heap.SearchFreeLists(kBlocks + 1, 0));

  // Free lists should contain a span that satisfies the request for kBlocks.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.SearchFreeLists(kBlocks, 0);
  ASSERT_TRUE(span);

  // Free lists should be empty again.
  EXPECT_FALSE(heap.SearchFreeLists(1, 0));

  // Merge it into the free lists again.
  heap.MergeIntoFreeLists(std::move(span));
}

TEST(DiscardableSharedMemoryHeapTest, SplitAndMerge) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  const size_t kBlocks = 6;
  size_t memory_size = block_size * kBlocks;
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap.Grow(std::move(memory), memory_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask)));

  // Split span into two.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> leftover =
      heap.Split(new_span.get(), 3);
  ASSERT_TRUE(leftover);

  // Merge |leftover| into free lists.
  heap.MergeIntoFreeLists(std::move(leftover));

  // Some of the memory is still in use.
  EXPECT_FALSE(heap.SearchFreeLists(kBlocks, 0));

  // Merge |span| into free lists.
  heap.MergeIntoFreeLists(std::move(new_span));

  // Remove a 2 page span from free lists.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span1 =
      heap.SearchFreeLists(2, kBlocks);
  ASSERT_TRUE(span1);

  // Remove another 2 page span from free lists.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span2 =
      heap.SearchFreeLists(2, kBlocks);
  ASSERT_TRUE(span2);

  // Merge |span1| back into free lists.
  heap.MergeIntoFreeLists(std::move(span1));

  // Some of the memory is still in use.
  EXPECT_FALSE(heap.SearchFreeLists(kBlocks, 0));

  // Merge |span2| back into free lists.
  heap.MergeIntoFreeLists(std::move(span2));

  // All memory has been returned to the free lists.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> large_span =
      heap.SearchFreeLists(kBlocks, 0);
  ASSERT_TRUE(large_span);

  // Merge it into the free lists again.
  heap.MergeIntoFreeLists(std::move(large_span));
}

TEST(DiscardableSharedMemoryHeapTest, MergeSingleBlockSpan) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);

  const size_t kBlocks = 6;
  size_t memory_size = block_size * kBlocks;
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap.Grow(std::move(memory), memory_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask)));

  // Split span into two.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> leftover =
      heap.Split(new_span.get(), 5);
  ASSERT_TRUE(leftover);

  // Merge |new_span| into free lists.
  heap.MergeIntoFreeLists(std::move(new_span));

  // Merge |leftover| into free lists.
  heap.MergeIntoFreeLists(std::move(leftover));
}

TEST(DiscardableSharedMemoryHeapTest, Grow) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory1(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory1->CreateAndMap(block_size));
  heap.MergeIntoFreeLists(heap.Grow(std::move(memory1), block_size,
                                    next_discardable_shared_memory_id++,
                                    base::BindOnce(NullTask)));

  // Remove a span from free lists.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span1 =
      heap.SearchFreeLists(1, 0);
  EXPECT_TRUE(span1);

  // No more memory available.
  EXPECT_FALSE(heap.SearchFreeLists(1, 0));

  // Grow free lists using new memory.
  std::unique_ptr<base::DiscardableSharedMemory> memory2(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory2->CreateAndMap(block_size));
  heap.MergeIntoFreeLists(heap.Grow(std::move(memory2), block_size,
                                    next_discardable_shared_memory_id++,
                                    base::BindOnce(NullTask)));

  // Memory should now be available.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span2 =
      heap.SearchFreeLists(1, 0);
  EXPECT_TRUE(span2);

  // Merge spans into the free lists again.
  heap.MergeIntoFreeLists(std::move(span1));
  heap.MergeIntoFreeLists(std::move(span2));
}

TEST(DiscardableSharedMemoryHeapTest, ReleaseFreeMemory) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.Grow(std::move(memory), block_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask));

  // Free lists should be empty.
  EXPECT_EQ(0u, heap.GetSizeOfFreeLists());

  heap.ReleaseFreeMemory();

  // Size should still match |block_size|.
  EXPECT_EQ(block_size, heap.GetSize());

  heap.MergeIntoFreeLists(std::move(span));
  heap.ReleaseFreeMemory();

  // Memory should have been released.
  EXPECT_EQ(0u, heap.GetSize());
  EXPECT_EQ(0u, heap.GetSizeOfFreeLists());
}

TEST(DiscardableSharedMemoryHeapTest, ReleasePurgedMemory) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.Grow(std::move(memory), block_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask));

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
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(memory_size));
  heap.MergeIntoFreeLists(heap.Grow(std::move(memory), memory_size,
                                    next_discardable_shared_memory_id++,
                                    base::BindOnce(NullTask)));

  // No free span that is less or equal to 3 + 1.
  EXPECT_FALSE(heap.SearchFreeLists(3, 1));

  // No free span that is less or equal to 3 + 2.
  EXPECT_FALSE(heap.SearchFreeLists(3, 2));

  // No free span that is less or equal to 1 + 4.
  EXPECT_FALSE(heap.SearchFreeLists(1, 4));

  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.SearchFreeLists(1, 5);
  EXPECT_TRUE(span);

  heap.MergeIntoFreeLists(std::move(span));
}

void OnDeleted(bool* deleted) {
  *deleted = true;
}

TEST(DiscardableSharedMemoryHeapTest, DeletedCallback) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  bool deleted = false;
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span = heap.Grow(
      std::move(memory), block_size, next_discardable_shared_memory_id++,
      base::BindOnce(OnDeleted, base::Unretained(&deleted)));

  heap.MergeIntoFreeLists(std::move(span));
  heap.ReleaseFreeMemory();

  EXPECT_TRUE(deleted);
}

TEST(DiscardableSharedMemoryHeapTest, CreateMemoryAllocatorDumpTest) {
  size_t block_size = base::GetPageSize();
  DiscardableSharedMemoryHeap heap(block_size);
  int next_discardable_shared_memory_id = 0;

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  ASSERT_TRUE(memory->CreateAndMap(block_size));
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
      heap.Grow(std::move(memory), block_size,
                next_discardable_shared_memory_id++, base::BindOnce(NullTask));

  // Check if allocator dump is created when span exists.
  std::unique_ptr<base::trace_event::ProcessMemoryDump> pmd(
      new base::trace_event::ProcessMemoryDump(
          {base::trace_event::MemoryDumpLevelOfDetail::DETAILED}));
  EXPECT_TRUE(heap.CreateMemoryAllocatorDump(span.get(), "discardable/test1",
                                             pmd.get()));

  // Unlock, Purge and release shared memory.
  span->shared_memory()->Unlock(0, 0);
  bool rv = span->shared_memory()->Purge(base::Time::Now());
  EXPECT_TRUE(rv);
  heap.ReleasePurgedMemory();

  // Check that allocator dump is created after memory is purged.
  EXPECT_TRUE(heap.CreateMemoryAllocatorDump(span.get(), "discardable/test2",
                                             pmd.get()));
}

}  // namespace
}  // namespace content

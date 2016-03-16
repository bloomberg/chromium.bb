// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/persistent_histogram_allocator.h"

#include "base/logging.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class PersistentHistogramAllocatorTest : public testing::Test {
 protected:
  const int32_t kAllocatorMemorySize = 64 << 10;  // 64 KiB

  PersistentHistogramAllocatorTest() { CreatePersistentHistogramAllocator(); }
  ~PersistentHistogramAllocatorTest() override {
    DestroyPersistentHistogramAllocator();
  }

  void CreatePersistentHistogramAllocator() {
    allocator_memory_.reset(new char[kAllocatorMemorySize]);

    PersistentHistogramAllocator::ReleaseGlobalAllocatorForTesting();
    memset(allocator_memory_.get(), 0, kAllocatorMemorySize);
    PersistentHistogramAllocator::GetCreateHistogramResultHistogram();
    PersistentHistogramAllocator::CreateGlobalAllocatorOnPersistentMemory(
        allocator_memory_.get(), kAllocatorMemorySize, 0, 0,
        "PersistentHistogramAllocatorTest");
    allocator_ =
        PersistentHistogramAllocator::GetGlobalAllocator()->memory_allocator();
  }

  void DestroyPersistentHistogramAllocator() {
    allocator_ = nullptr;
    PersistentHistogramAllocator::ReleaseGlobalAllocatorForTesting();
  }

  scoped_ptr<char[]> allocator_memory_;
  PersistentMemoryAllocator* allocator_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentHistogramAllocatorTest);
};

TEST_F(PersistentHistogramAllocatorTest, CreateAndIterateTest) {
  PersistentMemoryAllocator::MemoryInfo meminfo0;
  allocator_->GetMemoryInfo(&meminfo0);

  // Try basic construction
  HistogramBase* histogram = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, HistogramBase::kIsPersistent);
  EXPECT_TRUE(histogram);
  histogram->CheckName("TestHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo1;
  allocator_->GetMemoryInfo(&meminfo1);
  EXPECT_GT(meminfo0.free, meminfo1.free);

  HistogramBase* linear_histogram = LinearHistogram::FactoryGet(
      "TestLinearHistogram", 1, 1000, 10, HistogramBase::kIsPersistent);
  EXPECT_TRUE(linear_histogram);
  linear_histogram->CheckName("TestLinearHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo2;
  allocator_->GetMemoryInfo(&meminfo2);
  EXPECT_GT(meminfo1.free, meminfo2.free);

  HistogramBase* boolean_histogram = BooleanHistogram::FactoryGet(
      "TestBooleanHistogram", HistogramBase::kIsPersistent);
  EXPECT_TRUE(boolean_histogram);
  boolean_histogram->CheckName("TestBooleanHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo3;
  allocator_->GetMemoryInfo(&meminfo3);
  EXPECT_GT(meminfo2.free, meminfo3.free);

  std::vector<int> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(5);
  HistogramBase* custom_histogram = CustomHistogram::FactoryGet(
      "TestCustomHistogram", custom_ranges, HistogramBase::kIsPersistent);
  EXPECT_TRUE(custom_histogram);
  custom_histogram->CheckName("TestCustomHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo4;
  allocator_->GetMemoryInfo(&meminfo4);
  EXPECT_GT(meminfo3.free, meminfo4.free);

  PersistentMemoryAllocator::Iterator iter;
  uint32_t type;
  allocator_->CreateIterator(&iter);
  EXPECT_NE(0U, allocator_->GetNextIterable(&iter, &type));  // Histogram
  EXPECT_NE(0U, allocator_->GetNextIterable(&iter, &type));  // LinearHistogram
  EXPECT_NE(0U, allocator_->GetNextIterable(&iter, &type));  // BooleanHistogram
  EXPECT_NE(0U, allocator_->GetNextIterable(&iter, &type));  // CustomHistogram
  EXPECT_EQ(0U, allocator_->GetNextIterable(&iter, &type));

  // Create a second allocator and have it access the memory of the first.
  scoped_ptr<HistogramBase> recovered;
  PersistentHistogramAllocator recovery(
      make_scoped_ptr(new PersistentMemoryAllocator(
          allocator_memory_.get(), kAllocatorMemorySize, 0, 0, "", false)));
  PersistentHistogramAllocator::Iterator histogram_iter;
  recovery.CreateIterator(&histogram_iter);

  recovered = recovery.GetNextHistogram(&histogram_iter);
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestHistogram");

  recovered = recovery.GetNextHistogram(&histogram_iter);
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestLinearHistogram");

  recovered = recovery.GetNextHistogram(&histogram_iter);
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestBooleanHistogram");

  recovered = recovery.GetNextHistogram(&histogram_iter);
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestCustomHistogram");

  recovered = recovery.GetNextHistogram(&histogram_iter);
  EXPECT_FALSE(recovered);
}

}  // namespace base

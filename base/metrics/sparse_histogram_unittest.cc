// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/sparse_histogram.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sample_map.h"
#include "base/metrics/statistics_recorder.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Test parameter indicates if a persistent memory allocator should be used
// for histogram allocation. False will allocate histograms from the process
// heap.
class SparseHistogramTest : public testing::TestWithParam<bool> {
 protected:
  const int32_t kAllocatorMemorySize = 8 << 20;  // 8 MiB

  SparseHistogramTest() : use_persistent_histogram_allocator_(GetParam()) {}

  void SetUp() override {
    if (use_persistent_histogram_allocator_)
      CreatePersistentMemoryAllocator();

    // Each test will have a clean state (no Histogram / BucketRanges
    // registered).
    InitializeStatisticsRecorder();
  }

  void TearDown() override {
    if (allocator_) {
      ASSERT_FALSE(allocator_->IsFull());
      ASSERT_FALSE(allocator_->IsCorrupt());
    }
    UninitializeStatisticsRecorder();
    DestroyPersistentMemoryAllocator();
  }

  void InitializeStatisticsRecorder() {
    StatisticsRecorder::ResetForTesting();
    statistics_recorder_ = new StatisticsRecorder();
  }

  void UninitializeStatisticsRecorder() {
    delete statistics_recorder_;
    statistics_recorder_ = NULL;
  }

  void CreatePersistentMemoryAllocator() {
    // By getting the results-histogram before any persistent allocator
    // is attached, that histogram is guaranteed not to be stored in
    // any persistent memory segment (which simplifies some tests).
    PersistentHistogramAllocator::GetCreateHistogramResultHistogram();

    PersistentHistogramAllocator::CreateGlobalAllocatorOnLocalMemory(
        kAllocatorMemorySize, 0, "SparseHistogramAllocatorTest");
    allocator_ =
        PersistentHistogramAllocator::GetGlobalAllocator()->memory_allocator();
  }

  void DestroyPersistentMemoryAllocator() {
    allocator_ = nullptr;
    PersistentHistogramAllocator::ReleaseGlobalAllocatorForTesting();
  }

  scoped_ptr<SparseHistogram> NewSparseHistogram(const std::string& name) {
    return scoped_ptr<SparseHistogram>(new SparseHistogram(name));
  }

  const bool use_persistent_histogram_allocator_;

  StatisticsRecorder* statistics_recorder_;
  scoped_ptr<char[]> allocator_memory_;
  PersistentMemoryAllocator* allocator_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(SparseHistogramTest);
};

// Run all HistogramTest cases with both heap and persistent memory.
INSTANTIATE_TEST_CASE_P(HeapAndPersistent,
                        SparseHistogramTest,
                        testing::Bool());


TEST_P(SparseHistogramTest, BasicTest) {
  scoped_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  scoped_ptr<HistogramSamples> snapshot(histogram->SnapshotSamples());
  EXPECT_EQ(0, snapshot->TotalCount());
  EXPECT_EQ(0, snapshot->sum());

  histogram->Add(100);
  scoped_ptr<HistogramSamples> snapshot1(histogram->SnapshotSamples());
  EXPECT_EQ(1, snapshot1->TotalCount());
  EXPECT_EQ(1, snapshot1->GetCount(100));

  histogram->Add(100);
  histogram->Add(101);
  scoped_ptr<HistogramSamples> snapshot2(histogram->SnapshotSamples());
  EXPECT_EQ(3, snapshot2->TotalCount());
  EXPECT_EQ(2, snapshot2->GetCount(100));
  EXPECT_EQ(1, snapshot2->GetCount(101));
}

TEST_P(SparseHistogramTest, BasicTestAddCount) {
  scoped_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  scoped_ptr<HistogramSamples> snapshot(histogram->SnapshotSamples());
  EXPECT_EQ(0, snapshot->TotalCount());
  EXPECT_EQ(0, snapshot->sum());

  histogram->AddCount(100, 15);
  scoped_ptr<HistogramSamples> snapshot1(histogram->SnapshotSamples());
  EXPECT_EQ(15, snapshot1->TotalCount());
  EXPECT_EQ(15, snapshot1->GetCount(100));

  histogram->AddCount(100, 15);
  histogram->AddCount(101, 25);
  scoped_ptr<HistogramSamples> snapshot2(histogram->SnapshotSamples());
  EXPECT_EQ(55, snapshot2->TotalCount());
  EXPECT_EQ(30, snapshot2->GetCount(100));
  EXPECT_EQ(25, snapshot2->GetCount(101));
}

TEST_P(SparseHistogramTest, AddCount_LargeValuesDontOverflow) {
  scoped_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  scoped_ptr<HistogramSamples> snapshot(histogram->SnapshotSamples());
  EXPECT_EQ(0, snapshot->TotalCount());
  EXPECT_EQ(0, snapshot->sum());

  histogram->AddCount(1000000000, 15);
  scoped_ptr<HistogramSamples> snapshot1(histogram->SnapshotSamples());
  EXPECT_EQ(15, snapshot1->TotalCount());
  EXPECT_EQ(15, snapshot1->GetCount(1000000000));

  histogram->AddCount(1000000000, 15);
  histogram->AddCount(1010000000, 25);
  scoped_ptr<HistogramSamples> snapshot2(histogram->SnapshotSamples());
  EXPECT_EQ(55, snapshot2->TotalCount());
  EXPECT_EQ(30, snapshot2->GetCount(1000000000));
  EXPECT_EQ(25, snapshot2->GetCount(1010000000));
  EXPECT_EQ(55250000000LL, snapshot2->sum());
}

TEST_P(SparseHistogramTest, MacroBasicTest) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Sparse", 100);
  UMA_HISTOGRAM_SPARSE_SLOWLY("Sparse", 200);
  UMA_HISTOGRAM_SPARSE_SLOWLY("Sparse", 100);

  StatisticsRecorder::Histograms histograms;
  StatisticsRecorder::GetHistograms(&histograms);

  ASSERT_EQ(1U, histograms.size());
  HistogramBase* sparse_histogram = histograms[0];

  EXPECT_EQ(SPARSE_HISTOGRAM, sparse_histogram->GetHistogramType());
  EXPECT_EQ("Sparse", sparse_histogram->histogram_name());
  EXPECT_EQ(
      HistogramBase::kUmaTargetedHistogramFlag |
          (use_persistent_histogram_allocator_ ? HistogramBase::kIsPersistent
                                               : 0),
      sparse_histogram->flags());

  scoped_ptr<HistogramSamples> samples = sparse_histogram->SnapshotSamples();
  EXPECT_EQ(3, samples->TotalCount());
  EXPECT_EQ(2, samples->GetCount(100));
  EXPECT_EQ(1, samples->GetCount(200));
}

TEST_P(SparseHistogramTest, MacroInLoopTest) {
  // Unlike the macros in histogram.h, SparseHistogram macros can have a
  // variable as histogram name.
  for (int i = 0; i < 2; i++) {
    std::string name = StringPrintf("Sparse%d", i + 1);
    UMA_HISTOGRAM_SPARSE_SLOWLY(name, 100);
  }

  StatisticsRecorder::Histograms histograms;
  StatisticsRecorder::GetHistograms(&histograms);
  ASSERT_EQ(2U, histograms.size());

  std::string name1 = histograms[0]->histogram_name();
  std::string name2 = histograms[1]->histogram_name();
  EXPECT_TRUE(("Sparse1" == name1 && "Sparse2" == name2) ||
              ("Sparse2" == name1 && "Sparse1" == name2));
}

TEST_P(SparseHistogramTest, Serialize) {
  scoped_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  histogram->SetFlags(HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  histogram->SerializeInfo(&pickle);

  PickleIterator iter(pickle);

  int type;
  EXPECT_TRUE(iter.ReadInt(&type));
  EXPECT_EQ(SPARSE_HISTOGRAM, type);

  std::string name;
  EXPECT_TRUE(iter.ReadString(&name));
  EXPECT_EQ("Sparse", name);

  int flag;
  EXPECT_TRUE(iter.ReadInt(&flag));
  EXPECT_EQ(HistogramBase::kIPCSerializationSourceFlag, flag);

  // No more data in the pickle.
  EXPECT_FALSE(iter.SkipBytes(1));
}

TEST_P(SparseHistogramTest, FactoryTime) {
  const int kTestCreateCount = 1 << 10;  // Must be power-of-2.
  const int kTestLookupCount = 100000;
  const int kTestAddCount = 100000;

  // Create all histogram names in advance for accurate timing below.
  std::vector<std::string> histogram_names;
  for (int i = 0; i < kTestCreateCount; ++i) {
    histogram_names.push_back(
        StringPrintf("TestHistogram.%d", i % kTestCreateCount));
  }

  // Calculate cost of creating histograms.
  TimeTicks create_start = TimeTicks::Now();
  for (int i = 0; i < kTestCreateCount; ++i)
    SparseHistogram::FactoryGet(histogram_names[i], HistogramBase::kNoFlags);
  TimeDelta create_ticks = TimeTicks::Now() - create_start;
  int64_t create_ms = create_ticks.InMilliseconds();

  VLOG(1) << kTestCreateCount << " histogram creations took " << create_ms
          << "ms or about "
          << (create_ms * 1000000) / kTestCreateCount
          << "ns each.";

  // Calculate cost of looking up existing histograms.
  TimeTicks lookup_start = TimeTicks::Now();
  for (int i = 0; i < kTestLookupCount; ++i) {
    // 6007 is co-prime with kTestCreateCount and so will do lookups in an
    // order less likely to be cacheable (but still hit them all) should the
    // underlying storage use the exact histogram name as the key.
    const int i_mult = 6007;
    static_assert(i_mult < INT_MAX / kTestCreateCount, "Multiplier too big");
    int index = (i * i_mult) & (kTestCreateCount - 1);
    SparseHistogram::FactoryGet(histogram_names[index],
                                HistogramBase::kNoFlags);
  }
  TimeDelta lookup_ticks = TimeTicks::Now() - lookup_start;
  int64_t lookup_ms = lookup_ticks.InMilliseconds();

  VLOG(1) << kTestLookupCount << " histogram lookups took " << lookup_ms
          << "ms or about "
          << (lookup_ms * 1000000) / kTestLookupCount
          << "ns each.";

  // Calculate cost of accessing histograms.
  HistogramBase* histogram =
      SparseHistogram::FactoryGet(histogram_names[0], HistogramBase::kNoFlags);
  ASSERT_TRUE(histogram);
  TimeTicks add_start = TimeTicks::Now();
  for (int i = 0; i < kTestAddCount; ++i)
    histogram->Add(i & 127);
  TimeDelta add_ticks = TimeTicks::Now() - add_start;
  int64_t add_ms = add_ticks.InMilliseconds();

  VLOG(1) << kTestAddCount << " histogram adds took " << add_ms
          << "ms or about "
          << (add_ms * 1000000) / kTestAddCount
          << "ns each.";
}

}  // namespace base

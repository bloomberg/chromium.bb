// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/subprocess_metrics_provider.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const uint32_t TEST_MEMORY_SIZE = 64 << 10;  // 64 KiB

class HistogramFlattenerDeltaRecorder : public base::HistogramFlattener {
 public:
  HistogramFlattenerDeltaRecorder() {}

  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override {
    recorded_delta_histogram_names_.push_back(histogram.histogram_name());
  }

  void InconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override {
    ASSERT_TRUE(false);
  }

  void UniqueInconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override {
    ASSERT_TRUE(false);
  }

  void InconsistencyDetectedInLoggedCount(int amount) override {
    ASSERT_TRUE(false);
  }

  std::vector<std::string> GetRecordedDeltaHistogramNames() {
    return recorded_delta_histogram_names_;
  }

 private:
  std::vector<std::string> recorded_delta_histogram_names_;

  DISALLOW_COPY_AND_ASSIGN(HistogramFlattenerDeltaRecorder);
};

}  // namespace

class SubprocessMetricsProviderTest : public testing::Test {
 protected:
  SubprocessMetricsProviderTest() {
    // Get this first so it isn't created inside a persistent allocator.
    base::PersistentHistogramAllocator::GetCreateHistogramResultHistogram();

    // RecordHistogramSnapshots needs to be called beause it uses a histogram
    // macro which caches a pointer to a histogram. If not done before setting
    // a persistent global allocator, then it would point into memory that
    // will go away. The easiest way to call it is through an existing utility
    // method.
    GetSnapshotHistogramCount();

    // Create a global allocator using a block of memory from the heap.
    base::GlobalHistogramAllocator::CreateWithLocalMemory(TEST_MEMORY_SIZE,
                                                          0, "");

    // Enable metrics reporting by default.
    provider_.OnRecordingEnabled();
  }

  ~SubprocessMetricsProviderTest() override {
    base::GlobalHistogramAllocator::ReleaseForTesting();
  }

  SubprocessMetricsProvider* provider() { return &provider_; }

  std::unique_ptr<base::PersistentHistogramAllocator> GetDuplicateAllocator() {
    base::GlobalHistogramAllocator* global_allocator =
        base::GlobalHistogramAllocator::Get();

    // Just wrap around the data segment in-use by the global allocator.
    return WrapUnique(new base::PersistentHistogramAllocator(
        WrapUnique(new base::PersistentMemoryAllocator(
            const_cast<void*>(global_allocator->data()),
            global_allocator->length(), 0, 0, "", false))));
  }

  size_t GetSnapshotHistogramCount() {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    snapshot_manager.StartDeltas();
    provider_.RecordHistogramSnapshots(&snapshot_manager);
    snapshot_manager.FinishDeltas();
    provider_.OnDidCreateMetricsLog();
    return flattener.GetRecordedDeltaHistogramNames().size();
  }

  void EnableRecording() { provider_.OnRecordingEnabled(); }
  void DisableRecording() { provider_.OnRecordingDisabled(); }

  void RegisterSubprocessAllocator(
      int id,
      std::unique_ptr<base::PersistentHistogramAllocator> allocator) {
    provider_.RegisterSubprocessAllocator(id, std::move(allocator));
  }

  void DeregisterSubprocessAllocator(int id) {
    provider_.DeregisterSubprocessAllocator(id);
  }

 private:
  SubprocessMetricsProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(SubprocessMetricsProviderTest);
};

TEST_F(SubprocessMetricsProviderTest, SnapshotMetrics) {
  base::HistogramBase* foo = base::Histogram::FactoryGet("foo", 1, 100, 10, 0);
  base::HistogramBase* bar = base::Histogram::FactoryGet("bar", 1, 100, 10, 0);
  foo->Add(42);
  bar->Add(84);

  // Register an allocator that duplicates the global allocator.
  RegisterSubprocessAllocator(123, GetDuplicateAllocator());

  // Recording should find the two histograms created in persistent memory.
  EXPECT_EQ(2U, GetSnapshotHistogramCount());

  // A second run should have nothing to produce.
  EXPECT_EQ(0U, GetSnapshotHistogramCount());

  // Create a new histogram and update existing ones. Should now report 3 items.
  base::HistogramBase* baz = base::Histogram::FactoryGet("baz", 1, 100, 10, 0);
  baz->Add(1969);
  foo->Add(10);
  bar->Add(20);
  EXPECT_EQ(3U, GetSnapshotHistogramCount());

  // Ensure that deregistering still keeps allocator around for final report.
  foo->Add(10);
  bar->Add(20);
  DeregisterSubprocessAllocator(123);
  EXPECT_EQ(2U, GetSnapshotHistogramCount());

  // Further snapshots should be empty even if things have changed.
  foo->Add(10);
  bar->Add(20);
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
}

TEST_F(SubprocessMetricsProviderTest, EnableDisable) {
  base::HistogramBase* foo = base::Histogram::FactoryGet("foo", 1, 100, 10, 0);
  base::HistogramBase* bar = base::Histogram::FactoryGet("bar", 1, 100, 10, 0);

  // Simulate some "normal" operation...
  RegisterSubprocessAllocator(123, GetDuplicateAllocator());
  foo->Add(42);
  bar->Add(84);
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  foo->Add(42);
  bar->Add(84);
  EXPECT_EQ(2U, GetSnapshotHistogramCount());

  // Ensure that disable/enable reporting won't affect "live" allocators.
  DisableRecording();
  EnableRecording();
  foo->Add(42);
  bar->Add(84);
  EXPECT_EQ(2U, GetSnapshotHistogramCount());

  // Ensure that allocators are released when reporting is disabled.
  DisableRecording();
  DeregisterSubprocessAllocator(123);
  EnableRecording();
  foo->Add(42);
  bar->Add(84);
  EXPECT_EQ(0U, GetSnapshotHistogramCount());

  // Ensure that allocators added when reporting disabled will work if enabled.
  DisableRecording();
  RegisterSubprocessAllocator(123, GetDuplicateAllocator());
  EnableRecording();
  foo->Add(42);
  bar->Add(84);
  EXPECT_EQ(2U, GetSnapshotHistogramCount());

  // Ensure that last-chance allocators are released if reporting is disabled.
  DeregisterSubprocessAllocator(123);
  DisableRecording();
  EnableRecording();
  foo->Add(42);
  bar->Add(84);
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
}

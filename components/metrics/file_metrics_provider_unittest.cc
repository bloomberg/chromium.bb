// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/file_metrics_provider.h"

#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kMetricsName[] = "TestMetrics";
const char kMetricsFilename[] = "file.metrics";
}  // namespace

namespace metrics {

class HistogramFlattenerDeltaRecorder : public base::HistogramFlattener {
 public:
  HistogramFlattenerDeltaRecorder() {}

  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override {
    recorded_delta_histogram_names_.push_back(histogram.histogram_name());
  }

  void InconsistencyDetected(base::HistogramBase::Inconsistency problem)
      override {
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

class FileMetricsProviderTest : public testing::Test {
 protected:
  FileMetricsProviderTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_),
        prefs_(new TestingPrefServiceSimple) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    FileMetricsProvider::RegisterPrefs(prefs_->registry(), kMetricsName);
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }
  base::FilePath temp_dir() { return temp_dir_.path(); }
  base::FilePath metrics_file() {
    return temp_dir_.path().AppendASCII(kMetricsFilename);
  }

  FileMetricsProvider* provider() {
    if (!provider_)
      provider_.reset(new FileMetricsProvider(task_runner_, prefs()));
    return provider_.get();
  }

  void RunTasks() {
    task_runner_->RunUntilIdle();
  }

  void CreateMetricsFileWithHistograms(int histogram_count) {
    // Get this first so it isn't created inside the persistent allocator.
    base::GlobalHistogramAllocator::GetCreateHistogramResultHistogram();

    base::GlobalHistogramAllocator::CreateWithLocalMemory(
        64 << 10, 0, kMetricsName);

    // Create both sparse and normal histograms in the allocator.
    base::SparseHistogram::FactoryGet("h0", 0)->Add(0);
    for (int i = 1; i < histogram_count; ++i) {
      base::Histogram::FactoryGet(base::StringPrintf("h%d", i), 1, 100, 10, 0)
          ->Add(i);
    }

    std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
        base::GlobalHistogramAllocator::ReleaseForTesting();
    base::PersistentMemoryAllocator* allocator =
        histogram_allocator->memory_allocator();
    base::File writer(metrics_file(),
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    ASSERT_TRUE(writer.IsValid());
    ASSERT_EQ(static_cast<int>(allocator->used()),
              writer.Write(0, (const char*)allocator->data(),
                           allocator->used()));
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<FileMetricsProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(FileMetricsProviderTest);
};

TEST_F(FileMetricsProviderTest, AccessMetrics) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(2);

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterFile(
      metrics_file(),
      FileMetricsProvider::FILE_HISTOGRAMS_ATOMIC,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN,
      kMetricsName);

  // Record embedded snapshots via snapshot-manager.
  provider()->OnDidCreateMetricsLog();
  RunTasks();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    snapshot_manager.StartDeltas();
    provider()->RecordHistogramSnapshots(&snapshot_manager);
    snapshot_manager.FinishDeltas();
    EXPECT_EQ(2U, flattener.GetRecordedDeltaHistogramNames().size());
  }

  // Make sure a second call to the snapshot-recorder doesn't break anything.
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    snapshot_manager.StartDeltas();
    provider()->RecordHistogramSnapshots(&snapshot_manager);
    snapshot_manager.FinishDeltas();
    EXPECT_EQ(0U, flattener.GetRecordedDeltaHistogramNames().size());
  }

  // Second full run on the same file should produce nothing.
  provider()->OnDidCreateMetricsLog();
  RunTasks();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    snapshot_manager.StartDeltas();
    provider()->RecordHistogramSnapshots(&snapshot_manager);
    snapshot_manager.FinishDeltas();
    EXPECT_EQ(0U, flattener.GetRecordedDeltaHistogramNames().size());
  }

  // Update the time-stamp of the file to indicate that it is "new" and
  // must be recorded.
  {
    base::File touch(metrics_file(),
                     base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    ASSERT_TRUE(touch.IsValid());
    base::Time next = base::Time::Now() + base::TimeDelta::FromSeconds(1);
    touch.SetTimes(next, next);
  }

  // This run should again have "new" histograms.
  provider()->OnDidCreateMetricsLog();
  RunTasks();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    snapshot_manager.StartDeltas();
    provider()->RecordHistogramSnapshots(&snapshot_manager);
    snapshot_manager.FinishDeltas();
    EXPECT_EQ(2U, flattener.GetRecordedDeltaHistogramNames().size());
  }
}

TEST_F(FileMetricsProviderTest, AccessInitialMetrics) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(2);

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterFile(
      metrics_file(),
      FileMetricsProvider::FILE_HISTOGRAMS_ATOMIC,
      FileMetricsProvider::ASSOCIATE_PREVIOUS_RUN,
      kMetricsName);

  // Record embedded snapshots via snapshot-manager.
  provider()->HasInitialStabilityMetrics();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    snapshot_manager.StartDeltas();
    provider()->RecordInitialHistogramSnapshots(&snapshot_manager);
    snapshot_manager.FinishDeltas();
    EXPECT_EQ(2U, flattener.GetRecordedDeltaHistogramNames().size());
  }

  // Second full run on the same file should produce nothing.
  provider()->OnDidCreateMetricsLog();
  RunTasks();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    snapshot_manager.StartDeltas();
    provider()->RecordInitialHistogramSnapshots(&snapshot_manager);
    snapshot_manager.FinishDeltas();
    EXPECT_EQ(0U, flattener.GetRecordedDeltaHistogramNames().size());
  }

  // A run for normal histograms should produce nothing.
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    snapshot_manager.StartDeltas();
    provider()->RecordHistogramSnapshots(&snapshot_manager);
    snapshot_manager.FinishDeltas();
    EXPECT_EQ(0U, flattener.GetRecordedDeltaHistogramNames().size());
  }
}

}  // namespace metrics

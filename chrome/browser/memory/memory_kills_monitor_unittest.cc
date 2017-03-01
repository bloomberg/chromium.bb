// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_kills_monitor.h"

#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "chrome/browser/memory/memory_kills_histogram.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {

using MemoryKillsMonitorTest = testing::Test;

TEST_F(MemoryKillsMonitorTest, LogLowMemoryKill) {
  MemoryKillsMonitor::LogLowMemoryKill("APP", 123);
  MemoryKillsMonitor::LogLowMemoryKill("APP", 100);
  MemoryKillsMonitor::LogLowMemoryKill("TAB", 10000);

  auto* histogram_count =
      base::StatisticsRecorder::FindHistogram("Arc.LowMemoryKiller.Count");
  ASSERT_TRUE(histogram_count);
  auto count_samples = histogram_count->SnapshotSamples();
  EXPECT_EQ(3, count_samples->TotalCount());
  EXPECT_EQ(1, count_samples->GetCount(1));
  EXPECT_EQ(1, count_samples->GetCount(2));
  EXPECT_EQ(1, count_samples->GetCount(3));

  auto* histogram_freed_size =
      base::StatisticsRecorder::FindHistogram("Arc.LowMemoryKiller.FreedSize");
  ASSERT_TRUE(histogram_freed_size);
  auto freed_size_samples = histogram_freed_size->SnapshotSamples();
  EXPECT_EQ(3, freed_size_samples->TotalCount());
  // 123 and 100 are in the same bucket.
  EXPECT_EQ(2, freed_size_samples->GetCount(123));
  EXPECT_EQ(2, freed_size_samples->GetCount(100));
  EXPECT_EQ(1, freed_size_samples->GetCount(10000));

  auto* histogram_time_delta =
      base::StatisticsRecorder::FindHistogram("Arc.LowMemoryKiller.TimeDelta");
  ASSERT_TRUE(histogram_time_delta);
  auto time_delta_samples = histogram_time_delta->SnapshotSamples();
  EXPECT_EQ(3, time_delta_samples->TotalCount());
  // First time delta is set to kMaxMemoryKillTimeDelta.
  EXPECT_EQ(1, time_delta_samples->GetCount(
      kMaxMemoryKillTimeDelta.InMilliseconds()));
  // Time delta for the other 2 events depends on Now() so we skip testing it
  // here.
}

TEST_F(MemoryKillsMonitorTest, TryMatchOomKillLine) {
  const char* sample_lines[] = {
      "3,3429,812967386,-;Out of memory: Kill process 8291 (handle-watcher-) "
      "score 674 or sacrifice child",
      "3,3431,812981331,-;Out of memory: Kill process 8271 (.gms.persistent) "
      "score 652 or sacrifice child",
      "3,3433,812993014,-;Out of memory: Kill process 9210 (lowpool[11]) "
      "score 653 or sacrifice child"
  };

  for (unsigned long i = 0; i < arraysize(sample_lines); ++i) {
    MemoryKillsMonitor::TryMatchOomKillLine(sample_lines[i]);
  }

  auto* histogram_count =
      base::StatisticsRecorder::FindHistogram("Arc.OOMKills.Count");
  ASSERT_TRUE(histogram_count);
  auto count_samples = histogram_count->SnapshotSamples();
  EXPECT_EQ(3, count_samples->TotalCount());
  EXPECT_EQ(1, count_samples->GetCount(1));
  EXPECT_EQ(1, count_samples->GetCount(2));
  EXPECT_EQ(1, count_samples->GetCount(3));

  auto* histogram_score =
      base::StatisticsRecorder::FindHistogram("Arc.OOMKills.Score");
  ASSERT_TRUE(histogram_score);
  auto score_samples = histogram_score->SnapshotSamples();
  EXPECT_EQ(3, score_samples->TotalCount());
  EXPECT_EQ(1, score_samples->GetCount(674));
  EXPECT_EQ(1, score_samples->GetCount(652));
  EXPECT_EQ(1, score_samples->GetCount(653));

  auto* histogram_time_delta =
      base::StatisticsRecorder::FindHistogram("Arc.OOMKills.TimeDelta");
  ASSERT_TRUE(histogram_time_delta);
  auto time_delta_samples = histogram_time_delta->SnapshotSamples();
  EXPECT_EQ(3, time_delta_samples->TotalCount());
  // First time delta is set to kMaxMemoryKillTimeDelta.
  EXPECT_EQ(1, time_delta_samples->GetCount(
      kMaxMemoryKillTimeDelta.InMilliseconds()));
  EXPECT_EQ(1, time_delta_samples->GetCount(11));
  EXPECT_EQ(1, time_delta_samples->GetCount(13));
}

}  // namespace memory

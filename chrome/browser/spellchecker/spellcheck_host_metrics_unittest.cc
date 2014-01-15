// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/statistics_delta_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
// For version specific disabled tests below (http://crbug.com/230534).
#include "base/win/windows_version.h"
#endif

class SpellcheckHostMetricsTest : public testing::Test {
 public:
  SpellcheckHostMetricsTest() {
  }

  static void SetUpTestCase() {
    base::StatisticsRecorder::Initialize();
  }

  virtual void SetUp() OVERRIDE {
    metrics_.reset(new SpellCheckHostMetrics);
  }

  SpellCheckHostMetrics* metrics() { return metrics_.get(); }
  void RecordWordCountsForTesting() { metrics_->RecordWordCounts(); }

 private:
  base::MessageLoop loop_;
  scoped_ptr<SpellCheckHostMetrics> metrics_;
};

TEST_F(SpellcheckHostMetricsTest, RecordEnabledStats) {
  const char kMetricName[] = "SpellCheck.Enabled";
  base::StatisticsDeltaReader statistics_delta_reader1;

  metrics()->RecordEnabledStats(false);

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader1.GetHistogramSamplesSinceCreation(kMetricName));
  EXPECT_EQ(1, samples->GetCount(0));
  EXPECT_EQ(0, samples->GetCount(1));

  base::StatisticsDeltaReader statistics_delta_reader2;

  metrics()->RecordEnabledStats(true);

  samples =
      statistics_delta_reader2.GetHistogramSamplesSinceCreation(kMetricName);
  EXPECT_EQ(0, samples->GetCount(0));
  EXPECT_EQ(1, samples->GetCount(1));
}

TEST_F(SpellcheckHostMetricsTest, CustomWordStats) {
#if defined(OS_WIN)
// Failing consistently on Win7. See crbug.com/230534.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    return;
#endif
  SpellCheckHostMetrics::RecordCustomWordCountStats(123);

  // Determine if test failures are due the statistics recorder not being
  // available or because the histogram just isn't there: crbug.com/230534.
  EXPECT_TRUE(base::StatisticsRecorder::IsActive());

  base::StatisticsDeltaReader statistics_delta_reader;

  SpellCheckHostMetrics::RecordCustomWordCountStats(23);

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          "SpellCheck.CustomWords"));
  EXPECT_EQ(23, samples->sum());
}

TEST_F(SpellcheckHostMetricsTest, RecordWordCountsDiscardsDuplicates) {
  // This test ensures that RecordWordCounts only records metrics if they
  // have changed from the last invocation.
  const char* histogramName[] = {
    "SpellCheck.CheckedWords",
    "SpellCheck.MisspelledWords",
    "SpellCheck.ReplacedWords",
    "SpellCheck.UniqueWords",
    "SpellCheck.ShownSuggestions"
  };

  // Ensure all histograms exist.
  metrics()->RecordCheckedWordStats(base::ASCIIToUTF16("test"), false);
  RecordWordCountsForTesting();

  // Start the reader.
  base::StatisticsDeltaReader statistics_delta_reader;

  // Nothing changed, so this invocation should not affect any histograms.
  RecordWordCountsForTesting();

  // Get samples for all affected histograms.
  scoped_ptr<base::HistogramSamples> samples;
  for (size_t i = 0; i < arraysize(histogramName); ++i) {
    samples = statistics_delta_reader.GetHistogramSamplesSinceCreation(
        histogramName[i]);
    EXPECT_EQ(0, samples->TotalCount());
  }
}

TEST_F(SpellcheckHostMetricsTest, RecordSpellingServiceStats) {
  const char kMetricName[] = "SpellCheck.SpellingService.Enabled";
  base::StatisticsDeltaReader statistics_delta_reader1;

  metrics()->RecordSpellingServiceStats(false);

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader1.GetHistogramSamplesSinceCreation(kMetricName));
  EXPECT_EQ(1, samples->GetCount(0));
  EXPECT_EQ(0, samples->GetCount(1));

  base::StatisticsDeltaReader statistics_delta_reader2;

  metrics()->RecordSpellingServiceStats(true);

  samples =
      statistics_delta_reader2.GetHistogramSamplesSinceCreation(kMetricName);
  EXPECT_EQ(0, samples->GetCount(0));
  EXPECT_EQ(1, samples->GetCount(1));
}

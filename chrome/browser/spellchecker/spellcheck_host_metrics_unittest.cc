// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Histogram;
using base::HistogramSamples;
using base::StatisticsRecorder;

class SpellcheckHostMetricsTest : public testing::Test {
 public:
  SpellcheckHostMetricsTest() : loop_(MessageLoop::TYPE_DEFAULT) {
  }

  virtual void SetUp() OVERRIDE {
    base::StatisticsRecorder::Initialize();
    metrics_.reset(new SpellCheckHostMetrics);
  }

  SpellCheckHostMetrics* metrics() { return metrics_.get(); }
  void RecordWordCountsForTesting() { metrics_->RecordWordCounts(); }

 private:
   MessageLoop loop_;
   scoped_ptr<SpellCheckHostMetrics> metrics_;
};

TEST_F(SpellcheckHostMetricsTest, RecordEnabledStats) {
  scoped_ptr<HistogramSamples> baseline;
  Histogram* histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.Enabled");
  if (histogram)
    baseline = histogram->SnapshotSamples();

  metrics()->RecordEnabledStats(false);

  histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.Enabled");
  ASSERT_TRUE(histogram != NULL);
  scoped_ptr<HistogramSamples> samples(histogram->SnapshotSamples());
  if (baseline.get())
    samples->Subtract(*baseline);
  EXPECT_EQ(1, samples->GetCount(0));
  EXPECT_EQ(0, samples->GetCount(1));

  baseline.reset(samples.release());

  metrics()->RecordEnabledStats(true);

  histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.Enabled");
  ASSERT_TRUE(histogram != NULL);
  samples = histogram->SnapshotSamples();
  samples->Subtract(*baseline);
  EXPECT_EQ(0, samples->GetCount(0));
  EXPECT_EQ(1, samples->GetCount(1));
}

TEST_F(SpellcheckHostMetricsTest, CustomWordStats) {
  metrics()->RecordCustomWordCountStats(123);

  Histogram* histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.CustomWords");
  ASSERT_TRUE(histogram != NULL);
  scoped_ptr<HistogramSamples> baseline = histogram->SnapshotSamples();

  metrics()->RecordCustomWordCountStats(23);
  histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.CustomWords");
  ASSERT_TRUE(histogram != NULL);
  scoped_ptr<HistogramSamples> samples = histogram->SnapshotSamples();

  samples->Subtract(*baseline);
  EXPECT_EQ(23,samples->sum());
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
  metrics()->RecordCheckedWordStats(string16(ASCIIToUTF16("test")), false);
  RecordWordCountsForTesting();

  // Get baselines for all affected histograms.
  scoped_ptr<HistogramSamples> baselines[arraysize(histogramName)];
  for (size_t i = 0; i < arraysize(histogramName); ++i) {
    Histogram* histogram =
        StatisticsRecorder::FindHistogram(histogramName[i]);
    if (histogram)
      baselines[i] = histogram->SnapshotSamples();
  }

  // Nothing changed, so this invocation should not affect any histograms.
  RecordWordCountsForTesting();

  // Get samples for all affected histograms.
  scoped_ptr<HistogramSamples> samples[arraysize(histogramName)];
  for (size_t i = 0; i < arraysize(histogramName); ++i) {
    Histogram* histogram =
        StatisticsRecorder::FindHistogram(histogramName[i]);
    ASSERT_TRUE(histogram != NULL);
    samples[i] = histogram->SnapshotSamples();
    if (baselines[i].get())
      samples[i]->Subtract(*baselines[i]);

    EXPECT_EQ(0, samples[i]->TotalCount());
  }
}

TEST_F(SpellcheckHostMetricsTest, RecordSpellingServiceStats) {
  const char kMetricName[] = "SpellCheck.SpellingService.Enabled";
  scoped_ptr<HistogramSamples> baseline;
  Histogram* histogram =
      StatisticsRecorder::FindHistogram(kMetricName);
  if (histogram)
    baseline = histogram->SnapshotSamples();

  metrics()->RecordSpellingServiceStats(false);

  histogram =
      StatisticsRecorder::FindHistogram(kMetricName);
  ASSERT_TRUE(histogram != NULL);
  scoped_ptr<HistogramSamples> samples(histogram->SnapshotSamples());
  if (baseline.get())
    samples->Subtract(*baseline);
  EXPECT_EQ(1, samples->GetCount(0));
  EXPECT_EQ(0, samples->GetCount(1));

  baseline.reset(samples.release());

  metrics()->RecordSpellingServiceStats(true);

  histogram =
      StatisticsRecorder::FindHistogram(kMetricName);
  ASSERT_TRUE(histogram != NULL);
  samples = histogram->SnapshotSamples();
  samples->Subtract(*baseline);
  EXPECT_EQ(0, samples->GetCount(0));
  EXPECT_EQ(1, samples->GetCount(1));
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_samples.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
// For version specific disabled tests below (http://crbug.com/230534).
#include "base/win/windows_version.h"
#endif

class SpellcheckHostMetricsTest : public testing::Test {
 public:
  SpellcheckHostMetricsTest() : loop_(base::MessageLoop::TYPE_DEFAULT) {
  }

  static void SetUpTestCase() {
    base::HistogramRecorder::Initialize();
  }

  virtual void SetUp() OVERRIDE {
    ResetHistogramRecorder();
    metrics_.reset(new SpellCheckHostMetrics);
  }

  void ResetHistogramRecorder() {
    histogram_recorder_.reset(new base::HistogramRecorder());
  }

  SpellCheckHostMetrics* metrics() { return metrics_.get(); }
  void RecordWordCountsForTesting() { metrics_->RecordWordCounts(); }

 protected:
  scoped_ptr<base::HistogramRecorder> histogram_recorder_;

 private:
  base::MessageLoop loop_;
  scoped_ptr<SpellCheckHostMetrics> metrics_;
};

TEST_F(SpellcheckHostMetricsTest, RecordEnabledStats) {
  const char kMetricName[] = "SpellCheck.Enabled";

  metrics()->RecordEnabledStats(false);

  scoped_ptr<base::HistogramSamples> samples(
      histogram_recorder_->GetHistogramSamplesSinceCreation(kMetricName));
  EXPECT_EQ(1, samples->GetCount(0));
  EXPECT_EQ(0, samples->GetCount(1));

  ResetHistogramRecorder();

  metrics()->RecordEnabledStats(true);

  samples =
      histogram_recorder_->GetHistogramSamplesSinceCreation(kMetricName);
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
  EXPECT_TRUE(base::HistogramRecorder::IsActive());

  ResetHistogramRecorder();

  SpellCheckHostMetrics::RecordCustomWordCountStats(23);

  scoped_ptr<base::HistogramSamples> samples(
      histogram_recorder_->GetHistogramSamplesSinceCreation(
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
  metrics()->RecordCheckedWordStats(ASCIIToUTF16("test"), false);
  RecordWordCountsForTesting();

  // Restart the recorder.
  ResetHistogramRecorder();

  // Nothing changed, so this invocation should not affect any histograms.
  RecordWordCountsForTesting();

  // Get samples for all affected histograms.
  scoped_ptr<base::HistogramSamples> samples;
  for (size_t i = 0; i < arraysize(histogramName); ++i) {
    samples = histogram_recorder_->GetHistogramSamplesSinceCreation(
        histogramName[i]);
    EXPECT_EQ(0, samples->TotalCount());
  }
}

TEST_F(SpellcheckHostMetricsTest, RecordSpellingServiceStats) {
  const char kMetricName[] = "SpellCheck.SpellingService.Enabled";

  metrics()->RecordSpellingServiceStats(false);

  scoped_ptr<base::HistogramSamples> samples(
      histogram_recorder_->GetHistogramSamplesSinceCreation(kMetricName));
  EXPECT_EQ(1, samples->GetCount(0));
  EXPECT_EQ(0, samples->GetCount(1));

  ResetHistogramRecorder();

  metrics()->RecordSpellingServiceStats(true);

  samples =
      histogram_recorder_->GetHistogramSamplesSinceCreation(kMetricName);
  EXPECT_EQ(0, samples->GetCount(0));
  EXPECT_EQ(1, samples->GetCount(1));
}

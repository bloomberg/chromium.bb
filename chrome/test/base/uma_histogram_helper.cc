// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/uma_histogram_helper.h"

#include "base/bind.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/test_timeouts.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/histogram_fetcher.h"

UMAHistogramHelper::UMAHistogramHelper() {
  base::StatisticsRecorder::Initialize();
}

UMAHistogramHelper::~UMAHistogramHelper() {
}

void UMAHistogramHelper::PrepareSnapshot(const char* const histogram_names[],
                                         size_t num_histograms) {
  for (size_t i = 0; i < num_histograms; ++i) {
    std::string histogram_name = histogram_names[i];

    base::HistogramBase* histogram =
        base::StatisticsRecorder::FindHistogram(histogram_name);
    // If there is no histogram present, then don't record a snapshot. The logic
    // in the Expect* methods will act to treat no histogram equivalent to
    // samples with zeros.
    if (histogram) {
      histogram_snapshots[histogram_name] =
          make_linked_ptr(histogram->SnapshotSamples().release());
    }
  }
}

void UMAHistogramHelper::Fetch() {
  base::Closure callback = base::Bind(&UMAHistogramHelper::FetchCallback,
                                      base::Unretained(this));

  content::FetchHistogramsAsynchronously(
      base::MessageLoop::current(),
      callback,
      // If this call times out, it means that a child process is not
      // responding, which is something we should not ignore.  The timeout is
      // set to be longer than the normal browser test timeout so that it will
      // be prempted by the normal timeout.
      TestTimeouts::action_max_timeout() * 2);
  content::RunMessageLoop();
}

void UMAHistogramHelper::ExpectUniqueSample(
    const std::string& name,
    base::HistogramBase::Sample sample,
    base::HistogramBase::Count expected_count) {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  EXPECT_NE(static_cast<base::HistogramBase*>(NULL), histogram)
      << "Histogram \"" << name << "\" does not exist.";

  if (histogram) {
    scoped_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
    CheckBucketCount(name, sample, expected_count, *samples);
    CheckTotalCount(name, expected_count, *samples);
  }
}

void UMAHistogramHelper::ExpectBucketCount(
    const std::string& name,
    base::HistogramBase::Sample sample,
    base::HistogramBase::Count expected_count) {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  EXPECT_NE(static_cast<base::HistogramBase*>(NULL), histogram)
      << "Histogram \"" << name << "\" does not exist.";

  if (histogram) {
    scoped_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
    CheckBucketCount(name, sample, expected_count, *samples);
  }
}

void UMAHistogramHelper::ExpectTotalCount(
    const std::string& name,
    base::HistogramBase::Count count) {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (histogram) {
    scoped_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
    CheckTotalCount(name, count, *samples);
  } else {
    // No histogram means there were zero samples.
    EXPECT_EQ(count, 0) << "Histogram \"" << name << "\" does not exist.";
  }
}

void UMAHistogramHelper::FetchCallback() {
  base::MessageLoopForUI::current()->Quit();
}

void UMAHistogramHelper::CheckBucketCount(
    const std::string& name,
    base::HistogramBase::Sample sample,
    base::HistogramBase::Count expected_count,
    base::HistogramSamples& samples) {
  int actual_count = samples.GetCount(sample);
  if (histogram_snapshots.count(name))
    actual_count -= histogram_snapshots[name]->GetCount(sample);
  EXPECT_EQ(expected_count, actual_count)
      << "Histogram \"" << name
      << "\" does not have the right number of samples (" << expected_count
      << ") in the expected bucket (" << sample << "). It has (" << actual_count
      << ").";
}

void UMAHistogramHelper::CheckTotalCount(
    const std::string& name,
    base::HistogramBase::Count expected_count,
    base::HistogramSamples& samples) {
  int actual_count = samples.TotalCount();
  if (histogram_snapshots.count(name))
    actual_count -= histogram_snapshots[name]->TotalCount();
  EXPECT_EQ(expected_count, actual_count)
      << "Histogram \"" << name
      << "\" does not have the right total number of samples ("
      << expected_count << "). It has (" << actual_count << ").";
}

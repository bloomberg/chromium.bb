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
}

void UMAHistogramHelper::Fetch() {
  base::Closure callback = base::Bind(&UMAHistogramHelper::FetchCallback,
                                      base::Unretained(this));

  content::FetchHistogramsAsynchronously(
      MessageLoop::current(),
      callback,
      // If this call times out, it means that a child process is not
      // responding, which is something we should not ignore.  The timeout is
      // set to be longer than the normal browser test timeout so that it will
      // be prempted by the normal timeout.
      TestTimeouts::action_max_timeout()*2);
  content::RunMessageLoop();
}

void UMAHistogramHelper::ExpectUniqueSample(
    const std::string& name,
    size_t bucket_id,
    base::Histogram::Count expected_count) {
  base::Histogram* histogram = base::StatisticsRecorder::FindHistogram(name);
  EXPECT_NE(static_cast<base::Histogram*>(NULL), histogram) <<
      "Histogram \"" << name << "\" does not exist.";

  if (histogram) {
    base::Histogram::SampleSet samples;
    histogram->SnapshotSample(&samples);
    CheckBucketCount(name, bucket_id, expected_count, samples);
    CheckTotalCount(name, expected_count, samples);
  }
}

void UMAHistogramHelper::ExpectTotalCount(const std::string& name,
                                       base::Histogram::Count count) {
  base::Histogram* histogram = base::StatisticsRecorder::FindHistogram(name);
  EXPECT_NE((base::Histogram*)NULL, histogram) << "Histogram \"" << name <<
      "\" does not exist.";

  if (histogram) {
    base::Histogram::SampleSet samples;
    histogram->SnapshotSample(&samples);
    CheckTotalCount(name, count, samples);
  }
}

void UMAHistogramHelper::FetchCallback() {
  MessageLoopForUI::current()->Quit();
}

void UMAHistogramHelper::CheckBucketCount(const std::string& name,
                                       size_t bucket_id,
                                       base::Histogram::Count expected_count,
                                       base::Histogram::SampleSet& samples) {
  EXPECT_EQ(expected_count, samples.counts(bucket_id)) << "Histogram \"" <<
      name << "\" does not have the right number of samples (" <<
      expected_count << ") in the expected bucket (" << bucket_id << ").";
}

void UMAHistogramHelper::CheckTotalCount(const std::string& name,
                                      base::Histogram::Count expected_count,
                                      base::Histogram::SampleSet& samples) {
  EXPECT_EQ(expected_count, samples.TotalCount()) << "Histogram \"" << name <<
      "\" does not have the right total number of samples (" <<
      expected_count << ").";
}

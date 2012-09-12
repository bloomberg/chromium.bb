// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_UMA_HISTOGRAM_HELPER_H_
#define CHROME_TEST_BASE_UMA_HISTOGRAM_HELPER_H_

#include "base/metrics/histogram.h"

// UMAHistogramHelper provides a simple interface for examining UMA histograms.
// Tests can use this interface to verify that UMA data is getting logged as
// intended.
class UMAHistogramHelper {
 public:
  UMAHistogramHelper();

  // Each child process may have its own histogram data, make sure this data
  // gets accumulated into the browser process before we examine the histograms.
  void Fetch();

  // We know the exact number of samples in a bucket, and that no other bucket
  // should have samples.
  void ExpectUniqueSample(const std::string& name, size_t bucket_id,
                          base::Histogram::Count expected_count);

  // We don't know the values of the samples, but we know how many there are.
  void ExpectTotalCount(const std::string& name, base::Histogram::Count count);

 private:
  void FetchCallback();

  void CheckBucketCount(const std::string& name, size_t bucket_id,
                        base::Histogram::Count expected_count,
                        base::Histogram::SampleSet& samples);

  void CheckTotalCount(const std::string& name,
                       base::Histogram::Count expected_count,
                       base::Histogram::SampleSet& samples);

  DISALLOW_COPY_AND_ASSIGN(UMAHistogramHelper);
};

#endif  // CHROME_TEST_BASE_UMA_HISTOGRAM_HELPER_H_

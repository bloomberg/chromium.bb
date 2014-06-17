// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_UMA_HISTOGRAM_HELPER_H_
#define CHROME_TEST_BASE_UMA_HISTOGRAM_HELPER_H_

#include "base/memory/linked_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"

// UMAHistogramHelper provides a simple interface for examining UMA histograms.
// Tests can use this interface to verify that UMA data is getting logged as
// intended.
class UMAHistogramHelper {
 public:
  // UMAHistogramHelper should be created before the execution of the test case.
  UMAHistogramHelper();

  ~UMAHistogramHelper();

  // Parameters should be string literals of all histograms to snapshot.
  // Call this before executing the test code. This method can be called
  // multiple times. The existing snapshots are preserved, except when one of
  // the |histogram_names| was previously passed as a parameter, then a new
  // snapshot will replace the existing one.
  void PrepareSnapshot(const char* const histogram_names[],
                       size_t num_histograms);

  // Each child process may have its own histogram data, make sure this data
  // gets accumulated into the browser process before we examine the histograms.
  void Fetch();

  // We know the exact number of samples in a bucket, and that no other bucket
  // should have samples. If |PrepareSnapshot| was called for the histogram
  // named |name| then the |expected_count| is the diff from the snapshot.
  void ExpectUniqueSample(const std::string& name,
                          base::HistogramBase::Sample sample,
                          base::HistogramBase::Count expected_count);

  // We know the exact number of samples in a bucket, but other buckets may
  // have samples as well. If |PrepareSnapshot| was called for histogram named
  // |name| then the |expected_count| is the diff from the snapshot.
  void ExpectBucketCount(const std::string& name,
                          base::HistogramBase::Sample sample,
                          base::HistogramBase::Count expected_count);

  // We don't know the values of the samples, but we know how many there are. If
  // |PrepareSnapshot| was called for |name| histogram, then the
  // |count| is the diff from the snapshot.
  void ExpectTotalCount(const std::string& name,
                        base::HistogramBase::Count count);

 private:
  void FetchCallback();

  void CheckBucketCount(const std::string& name,
                        base::HistogramBase::Sample sample,
                        base::Histogram::Count expected_count,
                        base::HistogramSamples& samples);

  void CheckTotalCount(const std::string& name,
                       base::Histogram::Count expected_count,
                       base::HistogramSamples& samples);

  DISALLOW_COPY_AND_ASSIGN(UMAHistogramHelper);

  // The map from histogram names to their snapshots
  std::map<std::string, linked_ptr<base::HistogramSamples> >
      histogram_snapshots;
};

#endif  // CHROME_TEST_BASE_UMA_HISTOGRAM_HELPER_H_

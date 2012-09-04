// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"
#include "content/public/browser/histogram_fetcher.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"

namespace {

class HistogramHelper {
 public:
  HistogramHelper();

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
};

HistogramHelper::HistogramHelper() {
}

void HistogramHelper::Fetch() {
  base::Closure callback = base::Bind(&HistogramHelper::FetchCallback,
                                      base::Unretained(this));

  content::FetchHistogramsAsynchronously(
      MessageLoop::current(),
      callback,
      // Give up after 60 seconds, which is longer than the 45 second timeout
      // for browser tests.  If this call times out, it means that a child
      // process is not responding which is something we should not ignore.
      base::TimeDelta::FromMilliseconds(60000));
  content::RunMessageLoop();
}

void HistogramHelper::ExpectUniqueSample(
    const std::string& name,
    size_t bucket_id,
    base::Histogram::Count expected_count) {
  base::Histogram* histogram = base::StatisticsRecorder::FindHistogram(name);
  ASSERT_NE(static_cast<base::Histogram*>(NULL), histogram) <<
      "Histogram \"" << name << "\" does not exist.";

  base::Histogram::SampleSet samples;
  histogram->SnapshotSample(&samples);
  CheckBucketCount(name, bucket_id, expected_count, samples);
  CheckTotalCount(name, expected_count, samples);
}

void HistogramHelper::ExpectTotalCount(const std::string& name,
                                       base::Histogram::Count count) {
  base::Histogram* histogram = base::StatisticsRecorder::FindHistogram(name);
  ASSERT_NE((base::Histogram*)NULL, histogram) << "Histogram \"" << name <<
      "\" does not exist.";

  base::Histogram::SampleSet samples;
  histogram->SnapshotSample(&samples);
  CheckTotalCount(name, count, samples);
}

void HistogramHelper::FetchCallback() {
  MessageLoopForUI::current()->Quit();
}

void HistogramHelper::CheckBucketCount(const std::string& name,
                                       size_t bucket_id,
                                       base::Histogram::Count expected_count,
                                       base::Histogram::SampleSet& samples) {
  EXPECT_EQ(expected_count, samples.counts(bucket_id)) << "Histogram \"" <<
      name << "\" does not have the right number of samples (" <<
      expected_count << ") in the expected bucket (" << bucket_id << ").";
}

void HistogramHelper::CheckTotalCount(const std::string& name,
                                      base::Histogram::Count expected_count,
                                      base::Histogram::SampleSet& samples) {
  EXPECT_EQ(expected_count, samples.TotalCount()) << "Histogram \"" << name <<
      "\" does not have the right total number of samples (" <<
      expected_count << ").";
}

// Disable tests under Linux ASAN.
// Linux ASAN doesn't work with NaCl.  See: http://crbug.com/104832.
// TODO(ncbray) enable after http://codereview.chromium.org/10830009/ lands.
#if !(defined(ADDRESS_SANITIZER) && defined(OS_LINUX))

NACL_BROWSER_TEST_F(NaClBrowserTest, SuccessfulLoadUMA, {
  // Load a NaCl module to generate UMA data.
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));

  // Make sure histograms from child processes have been accumulated in the
  // browser brocess.
  HistogramHelper histograms;
  histograms.Fetch();

  // Did the plugin report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.Plugin",
                                plugin::ERROR_LOAD_SUCCESS, 1);

  // Did the sel_ldr report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.SelLdr",
                                LOAD_OK, 1);

  // Make sure we have other important histograms.
  histograms.ExpectTotalCount("NaCl.Perf.StartupTime.LoadModule", 1);
  histograms.ExpectTotalCount("NaCl.Perf.StartupTime.Total", 1);
  histograms.ExpectTotalCount("NaCl.Perf.Size.Manifest", 1);
  histograms.ExpectTotalCount("NaCl.Perf.Size.Nexe", 1);
})

// TODO(ncbray) convert the rest of nacl_uma.py (currently in the NaCl repo.)
// Test validation failures and crashes.

#endif  // !(defined(ADDRESS_SANITIZER) && defined(OS_LINUX))

}  // namespace anonymous

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_memory_details.h"

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/test/histogram_tester.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

class TestMemoryDetails : public MetricsMemoryDetails {
 public:
  TestMemoryDetails()
      : MetricsMemoryDetails(base::Bind(&base::DoNothing), nullptr) {}

  void StartFetchAndWait() {
    StartFetch();
    content::RunMessageLoop();
  }

 private:
  ~TestMemoryDetails() override {}

  void OnDetailsAvailable() override {
    MetricsMemoryDetails::OnDetailsAvailable();
    // Exit the loop initiated by StartFetchAndWait().
    base::MessageLoop::current()->QuitWhenIdle();
  }

  DISALLOW_COPY_AND_ASSIGN(TestMemoryDetails);
};

}  // namespace

class MetricsMemoryDetailsBrowserTest : public InProcessBrowserTest {
 public:
  MetricsMemoryDetailsBrowserTest() {}
  ~MetricsMemoryDetailsBrowserTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsMemoryDetailsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MetricsMemoryDetailsBrowserTest, TestMemoryDetails) {
  base::HistogramTester histogram_tester;

  scoped_refptr<TestMemoryDetails> details(new TestMemoryDetails);
  details->StartFetchAndWait();

  // Memory.Browser.Large2 and Memory.Browser.Committed histograms should each
  // have a single non-0 sample recorded.
  histogram_tester.ExpectTotalCount("Memory.Browser.Large2", 1);
  std::unique_ptr<base::HistogramSamples> samples(
      histogram_tester.GetHistogramSamplesSinceCreation(
          "Memory.Browser.Large2"));
  ASSERT_TRUE(samples);
  EXPECT_NE(0, samples->sum());

  histogram_tester.ExpectTotalCount("Memory.Browser.Committed", 1);
  std::unique_ptr<base::HistogramSamples> committed_samples(
      histogram_tester.GetHistogramSamplesSinceCreation(
          "Memory.Browser.Committed"));
  ASSERT_TRUE(committed_samples);
#if !defined(OS_LINUX)
  // Committed memory isn't calculated on Linux.
  EXPECT_NE(0, committed_samples->sum());
#endif
}

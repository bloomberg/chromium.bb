// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

const char kAdsMetricsFeature[] = "AdsMetrics";

class AdsPageLoadMetricsObserverBrowserTest : public InProcessBrowserTest {
 public:
  AdsPageLoadMetricsObserverBrowserTest() {
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII(switches::kEnableFeatures, kAdsMetricsFeature);
  }
  ~AdsPageLoadMetricsObserverBrowserTest() override {}

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserverBrowserTest);
};

// Test that a subframe that aborts (due to doc.write) doesn't cause a crash
// if it continues to load resources.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DocOverwritesNavigation) {
  content::DOMMessageQueue msg_queue;

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/docwrite_provisional_frame.html"));
  std::string status;
  EXPECT_TRUE(msg_queue.WaitForMessage(&status));
  EXPECT_EQ("\"loaded\"", status);

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total",
      0 /* < 1 KB */, 1);
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/histogram_tester.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/rappor/test_rappor_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace {

typedef InProcessBrowserTest NavigationMetricsRecorderBrowserTest;

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderBrowserTest, TestMetrics) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          web_contents);
  ASSERT_TRUE(recorder);
  rappor::TestRapporService rappor_service;
  recorder->set_rappor_service_for_testing(&rappor_service);

  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(),
                               GURL("data:text/html, <html></html>"));
  histograms.ExpectTotalCount("Navigation.MainFrameScheme", 1);
  histograms.ExpectBucketCount("Navigation.MainFrameScheme", 5 /* data: */, 1);
  // Since there was no previous URL, Rappor shouldn't record anything.
  EXPECT_EQ(0, rappor_service.GetReportsCount());

  // Navigate to an empty page and redirect it to a data: URL. Rappor should
  // record a report.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  content::TestNavigationObserver observer(web_contents, 1);
  EXPECT_TRUE(content::ExecuteScript(
      web_contents, "window.location.href='data:text/html, <html></html>'"));
  observer.Wait();

  EXPECT_EQ(1, rappor_service.GetReportsCount());
  std::string sample;
  rappor::RapporType type;
  EXPECT_TRUE(rappor_service.GetRecordedSampleForMetric(
      "Navigation.Scheme.Data", &sample, &type));
  EXPECT_EQ("about://", sample);
  EXPECT_EQ(rappor::ETLD_PLUS_ONE_RAPPOR_TYPE, type);
}

}  // namespace

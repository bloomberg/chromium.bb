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
  rappor::TestRapporServiceImpl rappor_service;
  recorder->set_rappor_service_for_testing(&rappor_service);

  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(),
                               GURL("data:text/html, <html></html>"));
  histograms.ExpectTotalCount("Navigation.MainFrameScheme", 1);
  histograms.ExpectBucketCount("Navigation.MainFrameScheme", 5 /* data: */, 1);
  histograms.ExpectTotalCount("Navigation.MainFrameSchemeDifferentPage", 1);
  histograms.ExpectBucketCount("Navigation.MainFrameSchemeDifferentPage",
                               5 /* data: */, 1);
  // Since there was no previous URL, Rappor shouldn't record anything.
  EXPECT_EQ(0, rappor_service.GetReportsCount());
}

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderBrowserTest, DataURLMimeTypes) {
  base::HistogramTester histograms;

  // HTML:
  ui_test_utils::NavigateToURL(browser(),
                               GURL("data:text/html, <html></html>"));
  histograms.ExpectTotalCount("Navigation.MainFrameScheme", 1);
  histograms.ExpectBucketCount("Navigation.MainFrameScheme", 5 /* data: */, 1);
  histograms.ExpectTotalCount("Navigation.MainFrameSchemeDifferentPage", 1);
  histograms.ExpectBucketCount("Navigation.MainFrameSchemeDifferentPage",
                               5 /* data: */, 1);
  histograms.ExpectTotalCount("Navigation.MainFrameScheme.DataUrl.MimeType", 0);

  // SVG:
  ui_test_utils::NavigateToURL(
      browser(), GURL("data:image/svg+xml,<!DOCTYPE svg><svg version=\"1.1\" "
                      "xmlns=\"http://www.w3.org/2000/svg\"></svg>"));
  histograms.ExpectTotalCount("Navigation.MainFrameScheme", 2);
  histograms.ExpectBucketCount("Navigation.MainFrameScheme", 5 /* data: */, 2);
  histograms.ExpectTotalCount("Navigation.MainFrameSchemeDifferentPage", 2);
  histograms.ExpectBucketCount("Navigation.MainFrameSchemeDifferentPage",
                               5 /* data: */, 2);
  histograms.ExpectTotalCount("Navigation.MainFrameScheme.DataUrl.MimeType", 0);

  // Base64 encoded HTML:
  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html;base64, PGh0bWw+YmFzZTY0PC9odG1sPg=="));
  histograms.ExpectTotalCount("Navigation.MainFrameScheme", 3);
  histograms.ExpectBucketCount("Navigation.MainFrameScheme", 5 /* data: */, 3);
  histograms.ExpectTotalCount("Navigation.MainFrameSchemeDifferentPage", 3);
  histograms.ExpectBucketCount("Navigation.MainFrameSchemeDifferentPage",
                               5 /* data: */, 3);
  histograms.ExpectTotalCount("Navigation.MainFrameScheme.DataUrl.MimeType", 0);

  // Plain text:
  ui_test_utils::NavigateToURL(browser(), GURL("data:text/plain, test"));
  histograms.ExpectTotalCount("Navigation.MainFrameScheme", 4);
  histograms.ExpectBucketCount("Navigation.MainFrameScheme", 5 /* data: */, 4);
  histograms.ExpectTotalCount("Navigation.MainFrameSchemeDifferentPage", 4);
  histograms.ExpectBucketCount("Navigation.MainFrameSchemeDifferentPage",
                               5 /* data: */, 4);
  histograms.ExpectTotalCount("Navigation.MainFrameScheme.DataUrl.MimeType", 0);

  // Base64 encoded PNG:
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA"
           "AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO"
           "9TXL0Y4OHwAAAABJRU5ErkJggg=="));
  histograms.ExpectTotalCount("Navigation.MainFrameScheme", 5);
  histograms.ExpectBucketCount("Navigation.MainFrameScheme", 5 /* data: */, 5);
  histograms.ExpectTotalCount("Navigation.MainFrameSchemeDifferentPage", 5);
  histograms.ExpectBucketCount("Navigation.MainFrameSchemeDifferentPage",
                               5 /* data: */, 5);
  histograms.ExpectTotalCount("Navigation.MainFrameScheme.DataUrl.MimeType", 0);
}

}  // namespace

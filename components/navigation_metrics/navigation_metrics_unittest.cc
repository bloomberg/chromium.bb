// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_metrics/navigation_metrics.h"

#include "base/test/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const char* const kTestUrl = "http://www.example.com";
const char* const kMainFrameScheme = "Navigation.MainFrameScheme";
const char* const kMainFrameSchemeDifferentPage =
    "Navigation.MainFrameSchemeDifferentPage";

}  // namespace

namespace navigation_metrics {

TEST(NavigationMetrics, MainFrameSchemeDifferentDocument) {
  base::HistogramTester test;

  RecordMainFrameNavigation(GURL(kTestUrl), false, false);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPage, 1 /* http */, 1);
}

TEST(NavigationMetrics, MainFrameSchemeSameDocument) {
  base::HistogramTester test;

  RecordMainFrameNavigation(GURL(kTestUrl), true, false);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 0);
}

}  // namespace navigation_metrics

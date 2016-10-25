// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace media_router {

TEST(MediaRouterMetricsTest, RecordMediaRouterDialogOrigin) {
  base::HistogramTester tester;
  const MediaRouterDialogOpenOrigin origin1 =
      MediaRouterDialogOpenOrigin::TOOLBAR;
  const MediaRouterDialogOpenOrigin origin2 =
      MediaRouterDialogOpenOrigin::CONTEXTUAL_MENU;

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramIconClickLocation, 0);
  MediaRouterMetrics::RecordMediaRouterDialogOrigin(origin1);
  MediaRouterMetrics::RecordMediaRouterDialogOrigin(origin2);
  MediaRouterMetrics::RecordMediaRouterDialogOrigin(origin1);
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramIconClickLocation, 3);
  EXPECT_THAT(
      tester.GetAllSamples(MediaRouterMetrics::kHistogramIconClickLocation),
      ElementsAre(Bucket(static_cast<int>(origin1), 2),
                  Bucket(static_cast<int>(origin2), 1)));
}

TEST(MediaRouterMetricsTest, RecordMediaRouterDialogPaint) {
  base::HistogramTester tester;
  const base::TimeDelta delta = base::TimeDelta::FromMilliseconds(10);

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramUiDialogPaint, 0);
  MediaRouterMetrics::RecordMediaRouterDialogPaint(delta);
  tester.ExpectUniqueSample(MediaRouterMetrics::kHistogramUiDialogPaint,
                            delta.InMilliseconds(), 1);
}

TEST(MediaRouterMetricsTest, RecordMediaRouterDialogLoaded) {
  base::HistogramTester tester;
  const base::TimeDelta delta = base::TimeDelta::FromMilliseconds(10);

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramUiDialogLoadedWithData,
                          0);
  MediaRouterMetrics::RecordMediaRouterDialogLoaded(delta);
  tester.ExpectUniqueSample(
      MediaRouterMetrics::kHistogramUiDialogLoadedWithData,
      delta.InMilliseconds(), 1);
}

TEST(MediaRouterMetricsTest, RecordMediaRouterInitialUserAction) {
  base::HistogramTester tester;
  const MediaRouterUserAction action1 = MediaRouterUserAction::START_LOCAL;
  const MediaRouterUserAction action2 = MediaRouterUserAction::CLOSE;
  const MediaRouterUserAction action3 = MediaRouterUserAction::STATUS_REMOTE;

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramUiFirstAction, 0);
  MediaRouterMetrics::RecordMediaRouterInitialUserAction(action3);
  MediaRouterMetrics::RecordMediaRouterInitialUserAction(action2);
  MediaRouterMetrics::RecordMediaRouterInitialUserAction(action3);
  MediaRouterMetrics::RecordMediaRouterInitialUserAction(action1);
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramUiFirstAction, 4);
  EXPECT_THAT(tester.GetAllSamples(MediaRouterMetrics::kHistogramUiFirstAction),
              ElementsAre(Bucket(static_cast<int>(action1), 1),
                          Bucket(static_cast<int>(action2), 1),
                          Bucket(static_cast<int>(action3), 2)));
}

TEST(MediaRouterMetricsTest, RecordRouteCreationOutcome) {
  base::HistogramTester tester;
  const MediaRouterRouteCreationOutcome outcome1 =
      MediaRouterRouteCreationOutcome::SUCCESS;
  const MediaRouterRouteCreationOutcome outcome2 =
      MediaRouterRouteCreationOutcome::FAILURE_NO_ROUTE;

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramRouteCreationOutcome,
                          0);
  MediaRouterMetrics::RecordRouteCreationOutcome(outcome2);
  MediaRouterMetrics::RecordRouteCreationOutcome(outcome1);
  MediaRouterMetrics::RecordRouteCreationOutcome(outcome2);
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramRouteCreationOutcome,
                          3);
  EXPECT_THAT(
      tester.GetAllSamples(MediaRouterMetrics::kHistogramRouteCreationOutcome),
      ElementsAre(Bucket(static_cast<int>(outcome1), 1),
                  Bucket(static_cast<int>(outcome2), 2)));
}

}  // namespace media_router

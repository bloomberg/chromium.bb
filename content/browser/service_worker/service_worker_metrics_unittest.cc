// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_metrics.h"

#include "base/test/histogram_tester.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
const std::string kNavigationPreloadSuffix = "_NavigationPreloadEnabled";
const std::string kStartWorkerDuringStartupSuffix = "_StartWorkerDuringStartup";
const std::string kStartWorkerExistingProcessSuffix =
    "_StartWorkerExistingProcess";
const std::string kPreparationTime =
    "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Time";
const std::string kPreparationType =
    "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type";
}  // namespace

TEST(ServiceWorkerMetricsTest, ActivatedWorkerPreparation) {
  base::TimeDelta time = base::TimeDelta::FromMilliseconds(123);
  {
    // Test preparation when the worker was STARTING.
    base::HistogramTester histogram_tester;
    ServiceWorkerMetrics::RecordActivatedWorkerPreparationForMainFrame(
        time, EmbeddedWorkerStatus::STARTING,
        ServiceWorkerMetrics::StartSituation::UNKNOWN,
        false /* did_navigation_preload */);
    histogram_tester.ExpectUniqueSample(
        kPreparationType,
        static_cast<int>(ServiceWorkerMetrics::WorkerPreparationType::STARTING),
        1);
    histogram_tester.ExpectTotalCount(
        kPreparationType + kNavigationPreloadSuffix, 0);
    histogram_tester.ExpectTimeBucketCount(kPreparationTime, time, 1);
    histogram_tester.ExpectTimeBucketCount(kPreparationTime + "_StartingWorker",
                                           time, 1);
    histogram_tester.ExpectTotalCount(
        kPreparationTime + kNavigationPreloadSuffix, 0);
  }

  {
    // Test preparation when the worker started up during startup.
    base::HistogramTester histogram_tester;
    ServiceWorkerMetrics::RecordActivatedWorkerPreparationForMainFrame(
        time, EmbeddedWorkerStatus::STOPPED,
        ServiceWorkerMetrics::StartSituation::DURING_STARTUP,
        true /* did_navigation_preload */);
    histogram_tester.ExpectUniqueSample(
        kPreparationType,
        static_cast<int>(
            ServiceWorkerMetrics::WorkerPreparationType::START_DURING_STARTUP),
        1);
    histogram_tester.ExpectUniqueSample(
        kPreparationType + kNavigationPreloadSuffix,
        static_cast<int>(
            ServiceWorkerMetrics::WorkerPreparationType::START_DURING_STARTUP),
        1);
    histogram_tester.ExpectTimeBucketCount(kPreparationTime, time, 1);
    histogram_tester.ExpectTimeBucketCount(
        kPreparationTime + kNavigationPreloadSuffix, time, 1);
    // This would be the correct name for such a histogram, but it's
    // intentionally not logged (see comment in
    // RecordActivatedWorkerPreparationForMainFrame), so expect zero count.
    histogram_tester.ExpectTotalCount(kPreparationTime +
                                          kStartWorkerDuringStartupSuffix +
                                          kNavigationPreloadSuffix,
                                      0);
    histogram_tester.ExpectTotalCount(kPreparationTime +
                                          kStartWorkerExistingProcessSuffix +
                                          kNavigationPreloadSuffix,
                                      0);
  }

  {
    // Test preparation when the worker started up in an existing process.
    base::HistogramTester histogram_tester;
    ServiceWorkerMetrics::RecordActivatedWorkerPreparationForMainFrame(
        time, EmbeddedWorkerStatus::STOPPED,
        ServiceWorkerMetrics::StartSituation::EXISTING_PROCESS,
        true /* did_navigation_preload */);
    histogram_tester.ExpectUniqueSample(
        kPreparationType,
        static_cast<int>(ServiceWorkerMetrics::WorkerPreparationType::
                             START_IN_EXISTING_PROCESS),
        1);
    histogram_tester.ExpectUniqueSample(
        kPreparationType + kNavigationPreloadSuffix,
        static_cast<int>(ServiceWorkerMetrics::WorkerPreparationType::
                             START_IN_EXISTING_PROCESS),
        1);
    histogram_tester.ExpectTimeBucketCount(kPreparationTime, time, 1);
    histogram_tester.ExpectTimeBucketCount(
        kPreparationTime + kNavigationPreloadSuffix, time, 1);
    histogram_tester.ExpectTimeBucketCount(
        kPreparationTime + kStartWorkerExistingProcessSuffix +
            kNavigationPreloadSuffix,
        time, 1);
  }
}

}  // namespace content

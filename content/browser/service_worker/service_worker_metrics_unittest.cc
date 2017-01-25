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

TEST(ServiceWorkerMetricsTest, NavigationPreloadResponse) {
  // The worker was already running.
  {
    base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(123);
    base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(789);
    EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::RUNNING;
    ServiceWorkerMetrics::StartSituation start_situation =
        ServiceWorkerMetrics::StartSituation::UNKNOWN;
    base::HistogramTester histogram_tester;
    ServiceWorkerMetrics::RecordNavigationPreloadResponse(
        worker_start, response_start, initial_worker_status, start_situation);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ResponseTime", response_start, 1);
    histogram_tester.ExpectUniqueSample(
        "ServiceWorker.NavigationPreload.FinishedBeforeStartWorker", false, 1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.NavigationPreload.FinishedBeforeStartWorker_"
        "StartWorkerExistingProcess",
        0);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime", worker_start, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime_SWStartFirst",
        worker_start, 1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime_SWStartFirst_"
        "StartWorkerExistingProcess",
        0);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.NavPreloadAfterSWStart",
        response_start - worker_start, 1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.NavigationPreload.NavPreloadAfterSWStart_"
        "StartWorkerExistingProcess",
        0);
  }

  // The worker had to start up.
  {
    base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(234);
    base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(789);
    EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::STOPPED;
    ServiceWorkerMetrics::StartSituation start_situation =
        ServiceWorkerMetrics::StartSituation::EXISTING_PROCESS;
    base::HistogramTester histogram_tester;
    ServiceWorkerMetrics::RecordNavigationPreloadResponse(
        worker_start, response_start, initial_worker_status, start_situation);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ResponseTime", response_start, 1);
    histogram_tester.ExpectUniqueSample(
        "ServiceWorker.NavigationPreload.FinishedBeforeStartWorker", false, 1);
    histogram_tester.ExpectUniqueSample(
        "ServiceWorker.NavigationPreload.FinishedBeforeStartWorker_"
        "StartWorkerExistingProcess",
        false, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime", worker_start, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime_SWStartFirst",
        worker_start, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime_SWStartFirst_"
        "StartWorkerExistingProcess",
        worker_start, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.NavPreloadAfterSWStart",
        response_start - worker_start, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.NavPreloadAfterSWStart_"
        "StartWorkerExistingProcess",
        response_start - worker_start, 1);
  }

  // The worker had to start up. But it happened during browser startup.
  {
    base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(456);
    base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(789);
    EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::STOPPED;
    ServiceWorkerMetrics::StartSituation start_situation =
        ServiceWorkerMetrics::StartSituation::DURING_STARTUP;
    base::HistogramTester histogram_tester;
    ServiceWorkerMetrics::RecordNavigationPreloadResponse(
        worker_start, response_start, initial_worker_status, start_situation);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ResponseTime", response_start, 1);
    histogram_tester.ExpectUniqueSample(
        "ServiceWorker.NavigationPreload.FinishedBeforeStartWorker", false, 1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.NavigationPreload.FinishedBeforeStartWorker_"
        "StartWorkerExistingProcess",
        0);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime", worker_start, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime_SWStartFirst",
        worker_start, 1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime_SWStartFirst_"
        "StartWorkerExistingProcess",
        0);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.NavPreloadAfterSWStart",
        response_start - worker_start, 1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.NavigationPreload.NavPreloadAfterSWStart_"
        "StartWorkerExistingProcess",
        0);
  }

  // The worker had to start up, and navigation preload finished first.
  {
    base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(2345);
    base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(456);
    EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::STOPPED;
    ServiceWorkerMetrics::StartSituation start_situation =
        ServiceWorkerMetrics::StartSituation::EXISTING_PROCESS;
    base::HistogramTester histogram_tester;
    ServiceWorkerMetrics::RecordNavigationPreloadResponse(
        worker_start, response_start, initial_worker_status, start_situation);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ResponseTime", response_start, 1);
    histogram_tester.ExpectUniqueSample(
        "ServiceWorker.NavigationPreload.FinishedBeforeStartWorker", true, 1);
    histogram_tester.ExpectUniqueSample(
        "ServiceWorker.NavigationPreload.FinishedBeforeStartWorker_"
        "StartWorkerExistingProcess",
        true, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime", response_start, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime_NavPreloadFirst",
        response_start, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.ConcurrentTime_NavPreloadFirst_"
        "StartWorkerExistingProcess",
        response_start, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.SWStartAfterNavPreload",
        worker_start - response_start, 1);
    histogram_tester.ExpectTimeBucketCount(
        "ServiceWorker.NavigationPreload.SWStartAfterNavPreload_"
        "StartWorkerExistingProcess",
        worker_start - response_start, 1);
  }
}

}  // namespace content

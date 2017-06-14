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
const std::string kWorkerStartOccurred = "_WorkerStartOccurred";
const std::string kStartWorkerExistingProcessSuffix =
    "_StartWorkerExistingProcess";
const std::string kPreparationTime =
    "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Time";
const std::string kPreparationType =
    "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type";

void ExpectNoNavPreloadMainFrameUMA(
    const base::HistogramTester& histogram_tester) {
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.ResponseTime_MainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame", 0);

  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.ResponseTime_MainFrame_"
      "WorkerStartOccurred",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame_"
      "WorkerStartOccurred",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame_"
      "WorkerStartOccurred",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame_"
      "WorkerStartOccurred",
      0);
}

void ExpectNoNavPreloadWorkerStartOccurredUMA(
    const base::HistogramTester& histogram_tester) {
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.ResponseTime_MainFrame_"
      "WorkerStartOccurred",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame_"
      "WorkerStartOccurred",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame_"
      "WorkerStartOccurred",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame_"
      "WorkerStartOccurred",
      0);
}

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
    histogram_tester.ExpectTotalCount(
        kPreparationTime + kWorkerStartOccurred + kNavigationPreloadSuffix, 1);
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
        kPreparationTime + kWorkerStartOccurred + kNavigationPreloadSuffix,
        time, 1);
  }
}

// ===========================================================================
// Navigation preload tests
// ===========================================================================
// The worker was already running.
TEST(ServiceWorkerMetricsTest,
     NavigationPreloadResponse_WorkerAlreadyRunning_MainFrame) {
  base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(123);
  base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(789);
  EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::RUNNING;
  base::HistogramTester histogram_tester;
  ServiceWorkerMetrics::RecordNavigationPreloadResponse(
      worker_start, response_start, initial_worker_status,
      RESOURCE_TYPE_MAIN_FRAME);

  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ResponseTime_MainFrame", response_start, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame", false, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame", worker_start, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame", 0);

  ExpectNoNavPreloadWorkerStartOccurredUMA(histogram_tester);
}

// The worker was already running (subframe).
TEST(ServiceWorkerMetricsTest,
     NavigationPreloadResponse_WorkerAlreadyRunning_SubFrame) {
  base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(123);
  base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(789);
  EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::RUNNING;
  base::HistogramTester histogram_tester;
  ServiceWorkerMetrics::RecordNavigationPreloadResponse(
      worker_start, response_start, initial_worker_status,
      RESOURCE_TYPE_SUB_FRAME);

  ExpectNoNavPreloadMainFrameUMA(histogram_tester);
}

// The worker started up.
TEST(ServiceWorkerMetricsTest,
     NavigationPreloadResponse_WorkerStart_MainFrame) {
  base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(234);
  base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(789);
  EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::STOPPED;
  base::HistogramTester histogram_tester;
  ServiceWorkerMetrics::RecordNavigationPreloadResponse(
      worker_start, response_start, initial_worker_status,
      RESOURCE_TYPE_MAIN_FRAME);

  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ResponseTime_MainFrame", response_start, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame", false, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame", worker_start, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame", 0);

  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ResponseTime_MainFrame_"
      "WorkerStartOccurred",
      response_start, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame_"
      "WorkerStartOccurred",
      false, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame_"
      "WorkerStartOccurred",
      worker_start, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame_"
      "WorkerStartOccurred",
      0);
}

// The worker started up (subframe).
TEST(ServiceWorkerMetricsTest, NavigationPreloadResponse_WorkerStart_SubFrame) {
  base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(234);
  base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(789);
  EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::STOPPED;
  base::HistogramTester histogram_tester;
  ServiceWorkerMetrics::RecordNavigationPreloadResponse(
      worker_start, response_start, initial_worker_status,
      RESOURCE_TYPE_SUB_FRAME);

  ExpectNoNavPreloadMainFrameUMA(histogram_tester);
}

// The worker was already starting up (STARTING). This gets logged the same as
// if it were STOPPED, since any status other than RUNNING gets logged to the
// "_WorkerStartOccurred" suffix.
TEST(ServiceWorkerMetricsTest,
     NavigationPreloadResponse_WorkerStart_BrowserStartup) {
  base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(234);
  base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(789);
  EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::STARTING;
  base::HistogramTester histogram_tester;
  ServiceWorkerMetrics::RecordNavigationPreloadResponse(
      worker_start, response_start, initial_worker_status,
      RESOURCE_TYPE_MAIN_FRAME);

  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ResponseTime_MainFrame", response_start, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame", false, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame", worker_start, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame", 0);

  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ResponseTime_MainFrame_"
      "WorkerStartOccurred",
      response_start, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame_"
      "WorkerStartOccurred",
      false, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame_"
      "WorkerStartOccurred",
      worker_start, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame_"
      "WorkerStartOccurred",
      0);
}

// The worker started up, but navigation preload arrived first.
TEST(ServiceWorkerMetricsTest,
     NavigationPreloadResponse_NavPreloadFinishedFirst) {
  base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(2345);
  base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(456);
  base::TimeDelta wait_time = worker_start - response_start;
  // STOPPING is also logged the same as STOPPED.
  EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::STOPPING;
  base::HistogramTester histogram_tester;
  ServiceWorkerMetrics::RecordNavigationPreloadResponse(
      worker_start, response_start, initial_worker_status,
      RESOURCE_TYPE_MAIN_FRAME);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ResponseTime_MainFrame", response_start, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame", true, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame", response_start, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame", wait_time, 1);

  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame_"
      "WorkerStartOccurred",
      true, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame_"
      "WorkerStartOccurred",
      response_start, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame_"
      "WorkerStartOccurred",
      wait_time, 1);
}

// The worker started up, but navigation preload arrived first (subframe).
TEST(ServiceWorkerMetricsTest,
     NavigationPreloadResponse_NavPreloadFinishedFirst_SubFrame) {
  base::TimeDelta worker_start = base::TimeDelta::FromMilliseconds(2345);
  base::TimeDelta response_start = base::TimeDelta::FromMilliseconds(456);
  EmbeddedWorkerStatus initial_worker_status = EmbeddedWorkerStatus::STOPPED;
  base::HistogramTester histogram_tester;
  ServiceWorkerMetrics::RecordNavigationPreloadResponse(
      worker_start, response_start, initial_worker_status,
      RESOURCE_TYPE_SUB_FRAME);

  ExpectNoNavPreloadMainFrameUMA(histogram_tester);
}

}  // namespace content

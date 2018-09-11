// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_event_dispatcher.h"

#include <stdint.h>
#include <memory>

#include "base/guid.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/background_fetch/background_fetch_embedded_worker_test_helper.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace content {
namespace {

const char kExampleDeveloperId[] = "my-id";
const char kExampleDeveloperId2[] = "my-second-id";
const char kExampleUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";
const char kExampleUniqueId2[] = "bb48a9fb-c21f-4c2d-a9ae-58bd48a9fb53";

class BackgroundFetchEventDispatcherTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchEventDispatcherTest()
      : event_dispatcher_(embedded_worker_test_helper()->context_wrapper()) {}
  ~BackgroundFetchEventDispatcherTest() override = default;

 protected:
  BackgroundFetchEventDispatcher event_dispatcher_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchEventDispatcherTest);
};

TEST_F(BackgroundFetchEventDispatcherTest, DispatchInvalidRegistration) {
  BackgroundFetchRegistrationId invalid_registration_id(
      9042 /* random invalid SW id */, origin(), kExampleDeveloperId,
      kExampleUniqueId);

  base::RunLoop run_loop;
  auto registration = CreateBackgroundFetchRegistration(
      invalid_registration_id.developer_id(),
      invalid_registration_id.unique_id(),
      blink::mojom::BackgroundFetchResult::FAILURE,
      blink::mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED);
  event_dispatcher_.DispatchBackgroundFetchAbortEvent(
      invalid_registration_id, std::move(registration), run_loop.QuitClosure());

  run_loop.Run();

  histogram_tester_.ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.AbortEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_CANNOT_FIND_WORKER, 1);
  histogram_tester_.ExpectBucketCount(
      "BackgroundFetch.EventDispatchFailure.FindWorker.AbortEvent",
      blink::ServiceWorkerStatusCode::kErrorNotFound, 1);
}

TEST_F(BackgroundFetchEventDispatcherTest, DispatchAbortEvent) {
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<BackgroundFetchSettledFetch> fetches;
  fetches.push_back(BackgroundFetchSettledFetch());

  BackgroundFetchRegistrationId registration_id(service_worker_registration_id,
                                                origin(), kExampleDeveloperId,
                                                kExampleUniqueId);

  {
    base::RunLoop run_loop;
    auto registration = CreateBackgroundFetchRegistration(
        kExampleDeveloperId, kExampleUniqueId,
        blink::mojom::BackgroundFetchResult::FAILURE,
        blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI);
    event_dispatcher_.DispatchBackgroundFetchAbortEvent(
        registration_id, std::move(registration), run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(embedded_worker_test_helper()->last_registration().has_value());
  EXPECT_EQ(kExampleDeveloperId,
            embedded_worker_test_helper()->last_registration()->developer_id);
  EXPECT_EQ(kExampleUniqueId,
            embedded_worker_test_helper()->last_registration()->unique_id);
  EXPECT_EQ(blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI,
            embedded_worker_test_helper()->last_registration()->failure_reason);

  histogram_tester_.ExpectUniqueSample(
      "BackgroundFetch.EventDispatchResult.AbortEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);

  embedded_worker_test_helper()->set_fail_abort_event(true);

  BackgroundFetchRegistrationId second_registration_id(
      service_worker_registration_id, origin(), kExampleDeveloperId2,
      kExampleUniqueId2);

  {
    base::RunLoop run_loop;
    auto registration = CreateBackgroundFetchRegistration(
        kExampleDeveloperId2, kExampleUniqueId2,
        blink::mojom::BackgroundFetchResult::FAILURE,
        blink::mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED);
    event_dispatcher_.DispatchBackgroundFetchAbortEvent(second_registration_id,
                                                        std::move(registration),
                                                        run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(embedded_worker_test_helper()->last_registration().has_value());
  EXPECT_EQ(kExampleDeveloperId2,
            embedded_worker_test_helper()->last_registration()->developer_id);
  EXPECT_EQ(kExampleUniqueId2,
            embedded_worker_test_helper()->last_registration()->unique_id);

  histogram_tester_.ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.AbortEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);
  histogram_tester_.ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.AbortEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_CANNOT_DISPATCH_EVENT, 1);
  histogram_tester_.ExpectUniqueSample(
      "BackgroundFetch.EventDispatchFailure.Dispatch.AbortEvent",
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected, 1);
}

TEST_F(BackgroundFetchEventDispatcherTest, DispatchClickEvent) {
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id(service_worker_registration_id,
                                                origin(), kExampleDeveloperId,
                                                kExampleUniqueId);

  {
    base::RunLoop run_loop;
    auto registration = CreateBackgroundFetchRegistration(
        kExampleDeveloperId, kExampleUniqueId,
        blink::mojom::BackgroundFetchResult::UNSET,
        blink::mojom::BackgroundFetchFailureReason::NONE);
    event_dispatcher_.DispatchBackgroundFetchClickEvent(
        registration_id, std::move(registration), run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(embedded_worker_test_helper()->last_registration().has_value());
  EXPECT_EQ(kExampleDeveloperId,
            embedded_worker_test_helper()->last_registration()->developer_id);
  EXPECT_EQ(blink::mojom::BackgroundFetchResult::UNSET,
            embedded_worker_test_helper()->last_registration()->result);

  histogram_tester_.ExpectUniqueSample(
      "BackgroundFetch.EventDispatchResult.ClickEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);

  embedded_worker_test_helper()->set_fail_click_event(true);

  BackgroundFetchRegistrationId second_registration_id(
      service_worker_registration_id, origin(), kExampleDeveloperId2,
      kExampleUniqueId2);

  {
    base::RunLoop run_loop;
    auto registration = CreateBackgroundFetchRegistration(
        kExampleDeveloperId2, kExampleUniqueId2,
        blink::mojom::BackgroundFetchResult::FAILURE,
        blink::mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED);
    event_dispatcher_.DispatchBackgroundFetchClickEvent(second_registration_id,
                                                        std::move(registration),
                                                        run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(embedded_worker_test_helper()->last_registration().has_value());
  EXPECT_EQ(kExampleDeveloperId2,
            embedded_worker_test_helper()->last_registration()->developer_id);
  EXPECT_EQ(blink::mojom::BackgroundFetchResult::FAILURE,
            embedded_worker_test_helper()->last_registration()->result);

  histogram_tester_.ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.ClickEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);
  histogram_tester_.ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.ClickEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_CANNOT_DISPATCH_EVENT, 1);
  histogram_tester_.ExpectUniqueSample(
      "BackgroundFetch.EventDispatchFailure.Dispatch.ClickEvent",
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected, 1);
}

TEST_F(BackgroundFetchEventDispatcherTest, DispatchFailEvent) {
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id(service_worker_registration_id,
                                                origin(), kExampleDeveloperId,
                                                kExampleUniqueId);

  {
    base::RunLoop run_loop;
    auto registration = CreateBackgroundFetchRegistration(
        kExampleDeveloperId, kExampleUniqueId,
        blink::mojom::BackgroundFetchResult::FAILURE,
        blink::mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED);
    event_dispatcher_.DispatchBackgroundFetchFailEvent(
        registration_id, std::move(registration), run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(embedded_worker_test_helper()->last_registration().has_value());
  EXPECT_EQ(kExampleDeveloperId,
            embedded_worker_test_helper()->last_registration()->developer_id);

  histogram_tester_.ExpectUniqueSample(
      "BackgroundFetch.EventDispatchResult.FailEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);

  embedded_worker_test_helper()->set_fail_fetch_fail_event(true);

  BackgroundFetchRegistrationId second_registration_id(
      service_worker_registration_id, origin(), kExampleDeveloperId2,
      kExampleUniqueId2);

  {
    base::RunLoop run_loop;
    auto registration = CreateBackgroundFetchRegistration(
        kExampleDeveloperId2, kExampleUniqueId2,
        blink::mojom::BackgroundFetchResult::FAILURE,
        blink::mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED);
    event_dispatcher_.DispatchBackgroundFetchFailEvent(second_registration_id,
                                                       std::move(registration),
                                                       run_loop.QuitClosure());
    run_loop.Run();
  }

  ASSERT_TRUE(embedded_worker_test_helper()->last_registration().has_value());
  EXPECT_EQ(kExampleDeveloperId2,
            embedded_worker_test_helper()->last_registration()->developer_id);

  histogram_tester_.ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.FailEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);
  histogram_tester_.ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.FailEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_CANNOT_DISPATCH_EVENT, 1);
  histogram_tester_.ExpectUniqueSample(
      "BackgroundFetch.EventDispatchFailure.Dispatch.FailEvent",
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected, 1);
}

TEST_F(BackgroundFetchEventDispatcherTest, DispatchFetchSuccessEvent) {
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id(service_worker_registration_id,
                                                origin(), kExampleDeveloperId,
                                                kExampleUniqueId);

  {
    base::RunLoop run_loop;
    auto registration = CreateBackgroundFetchRegistration(
        kExampleDeveloperId, kExampleUniqueId,
        blink::mojom::BackgroundFetchResult::SUCCESS,
        blink::mojom::BackgroundFetchFailureReason::NONE);
    event_dispatcher_.DispatchBackgroundFetchSuccessEvent(
        registration_id, std::move(registration), run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(embedded_worker_test_helper()->last_registration().has_value());
  EXPECT_EQ(kExampleDeveloperId,
            embedded_worker_test_helper()->last_registration()->developer_id);

  EXPECT_EQ(kExampleUniqueId,
            embedded_worker_test_helper()->last_registration()->unique_id);

  histogram_tester_.ExpectUniqueSample(
      "BackgroundFetch.EventDispatchResult.SuccessEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);

  embedded_worker_test_helper()->set_fail_fetched_event(true);

  BackgroundFetchRegistrationId second_registration_id(
      service_worker_registration_id, origin(), kExampleDeveloperId2,
      kExampleUniqueId2);

  {
    base::RunLoop run_loop;
    auto registration = CreateBackgroundFetchRegistration(
        kExampleDeveloperId2, kExampleUniqueId2,
        blink::mojom::BackgroundFetchResult::SUCCESS,
        blink::mojom::BackgroundFetchFailureReason::NONE);
    event_dispatcher_.DispatchBackgroundFetchSuccessEvent(
        second_registration_id, std::move(registration),
        run_loop.QuitClosure());

    run_loop.Run();
  }

  ASSERT_TRUE(embedded_worker_test_helper()->last_registration().has_value());
  EXPECT_EQ(kExampleDeveloperId2,
            embedded_worker_test_helper()->last_registration()->developer_id);
  EXPECT_EQ(kExampleUniqueId2,
            embedded_worker_test_helper()->last_registration()->unique_id);

  histogram_tester_.ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.SuccessEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_SUCCESS, 1);
  histogram_tester_.ExpectBucketCount(
      "BackgroundFetch.EventDispatchResult.SuccessEvent",
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_CANNOT_DISPATCH_EVENT, 1);
  histogram_tester_.ExpectUniqueSample(
      "BackgroundFetch.EventDispatchFailure.Dispatch.SuccessEvent",
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected, 1);
}

}  // namespace
}  // namespace content

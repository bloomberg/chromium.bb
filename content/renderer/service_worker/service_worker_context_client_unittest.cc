// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_client.h"

#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/service_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_timeout_timer.h"
#include "content/renderer/worker_thread_registry.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/common/message_port/message_port_channel.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/WebKit/public/platform/WebDataConsumerHandle.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/WebBackgroundFetchSettledFetch.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationData.h"
#include "third_party/WebKit/public/platform/modules/payments/WebPaymentRequestEventData.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"

namespace content {

namespace {

// Pipes connected to the context client.
struct ContextClientPipes {
  // From the browser to ServiceWorkerContextClient.
  mojom::ServiceWorkerEventDispatcherPtr event_dispatcher;
  mojom::ControllerServiceWorkerPtr controller;
  blink::mojom::ServiceWorkerRegistrationObjectAssociatedPtr registration;

  // From ServiceWorkerContextClient to the browser.
  blink::mojom::ServiceWorkerHostAssociatedRequest service_worker_host_request;
  mojom::EmbeddedWorkerInstanceHostAssociatedRequest
      embedded_worker_host_request;
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedRequest
      registration_host_request;
};

class MockWebServiceWorkerContextProxy
    : public blink::WebServiceWorkerContextProxy {
 public:
  ~MockWebServiceWorkerContextProxy() override = default;

  void SetRegistration(
      std::unique_ptr<blink::WebServiceWorkerRegistration::Handle> handle)
      override {
    registration_handle_ = std::move(handle);
  }
  bool HasFetchEventHandler() override { return false; }
  void DispatchFetchEvent(int fetch_event_id,
                          const blink::WebServiceWorkerRequest& web_request,
                          bool navigation_preload_sent) override {
    fetch_events_.emplace_back(fetch_event_id, web_request);
  }

  void DispatchActivateEvent(int event_id) override { NOTREACHED(); }
  void DispatchBackgroundFetchAbortEvent(
      int event_id,
      const blink::WebString& developer_id) override {
    NOTREACHED();
  }
  void DispatchBackgroundFetchClickEvent(int event_id,
                                         const blink::WebString& developer_id,
                                         BackgroundFetchState status) override {
    NOTREACHED();
  }
  void DispatchBackgroundFetchFailEvent(
      int event_id,
      const blink::WebString& developer_id,
      const blink::WebVector<blink::WebBackgroundFetchSettledFetch>& fetches)
      override {
    NOTREACHED();
  }
  void DispatchBackgroundFetchedEvent(
      int event_id,
      const blink::WebString& developer_id,
      const blink::WebString& unique_id,
      const blink::WebVector<blink::WebBackgroundFetchSettledFetch>& fetches)
      override {
    NOTREACHED();
  }
  void DispatchExtendableMessageEvent(
      int event_id,
      blink::TransferableMessage message,
      const blink::WebSecurityOrigin& source_origin,
      const blink::WebServiceWorkerClientInfo&) override {
    NOTREACHED();
  }
  void DispatchExtendableMessageEvent(
      int event_id,
      blink::TransferableMessage message,
      const blink::WebSecurityOrigin& source_origin,
      std::unique_ptr<blink::WebServiceWorker::Handle>) override {
    NOTREACHED();
  }
  void DispatchInstallEvent(int event_id) override { NOTREACHED(); }
  void DispatchNotificationClickEvent(int event_id,
                                      const blink::WebString& notification_id,
                                      const blink::WebNotificationData&,
                                      int action_index,
                                      const blink::WebString& reply) override {
    NOTREACHED();
  }
  void DispatchNotificationCloseEvent(
      int event_id,
      const blink::WebString& notification_id,
      const blink::WebNotificationData&) override {
    NOTREACHED();
  }
  void DispatchPushEvent(int event_id, const blink::WebString& data) override {
    NOTREACHED();
  }
  void DispatchSyncEvent(int sync_event_id,
                         const blink::WebString& tag,
                         bool last_chance) override {
    NOTREACHED();
  }
  void DispatchAbortPaymentEvent(int event_id) override { NOTREACHED(); }
  void DispatchCanMakePaymentEvent(
      int event_id,
      const blink::WebCanMakePaymentEventData&) override {
    NOTREACHED();
  }
  void DispatchPaymentRequestEvent(
      int event_id,
      const blink::WebPaymentRequestEventData&) override {
    NOTREACHED();
  }
  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<blink::WebURLResponse>,
      std::unique_ptr<blink::WebDataConsumerHandle>) override {
    NOTREACHED();
  }
  void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<blink::WebServiceWorkerError>) override {
    NOTREACHED();
  }
  void OnNavigationPreloadComplete(int fetch_event_id,
                                   double completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length) override {
    NOTREACHED();
  }

  const std::vector<
      std::pair<int /* event_id */, blink::WebServiceWorkerRequest>>&
  fetch_events() const {
    return fetch_events_;
  }

 private:
  std::unique_ptr<blink::WebServiceWorkerRegistration::Handle>
      registration_handle_;
  std::vector<std::pair<int /* event_id */, blink::WebServiceWorkerRequest>>
      fetch_events_;
};

base::RepeatingClosure CreateCallbackWithCalledFlag(bool* out_is_called) {
  return base::BindRepeating([](bool* out_is_called) { *out_is_called = true; },
                             out_is_called);
}

}  // namespace

class ServiceWorkerContextClientTest : public testing::Test {
 public:
  ServiceWorkerContextClientTest() = default;

 protected:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    message_loop_.SetTaskRunner(task_runner_);
    // Use this thread as the worker thread.
    WorkerThreadRegistry::Instance()->DidStartCurrentWorkerThread();
  }

  void TearDown() override {
    ServiceWorkerContextClient::ResetThreadSpecificInstanceForTesting();
    ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance()
        ->AllowReinstantiationForTesting();
    // Unregister this thread from worker threads.
    WorkerThreadRegistry::Instance()->WillStopCurrentWorkerThread();
  }

  void EnableServicification() {
    feature_list_.InitWithFeatures({network::features::kNetworkService}, {});
    ASSERT_TRUE(ServiceWorkerUtils::IsServicificationEnabled());
  }

  // Creates an empty struct to initialize ServiceWorkerProviderContext.
  mojom::ServiceWorkerProviderInfoForStartWorkerPtr CreateProviderInfo(
      blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedRequest*
          out_request,
      blink::mojom::ServiceWorkerRegistrationObjectAssociatedPtr* out_ptr) {
    auto info = mojom::ServiceWorkerProviderInfoForStartWorker::New();
    info->registration =
        blink::mojom::ServiceWorkerRegistrationObjectInfo::New();
    blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedPtr host_ptr;
    *out_request = mojo::MakeRequestAssociatedWithDedicatedPipe(&host_ptr);
    info->registration->host_ptr_info = host_ptr.PassInterface();
    info->registration->request =
        mojo::MakeRequestAssociatedWithDedicatedPipe(out_ptr);
    info->registration->registration_id = 100;  // dummy
    return info;
  }

  // Creates an ContextClient, whose pipes are connected to |out_pipes|.
  std::unique_ptr<ServiceWorkerContextClient> CreateContextClient(
      ContextClientPipes* out_pipes) {
    auto event_dispatcher_request =
        mojo::MakeRequest(&out_pipes->event_dispatcher);
    auto controller_request = mojo::MakeRequest(&out_pipes->controller);
    blink::mojom::ServiceWorkerHostAssociatedPtr sw_host_ptr;
    out_pipes->service_worker_host_request =
        mojo::MakeRequestAssociatedWithDedicatedPipe(&sw_host_ptr);
    mojom::EmbeddedWorkerInstanceHostAssociatedPtr embedded_worker_host_ptr;
    out_pipes->embedded_worker_host_request =
        mojo::MakeRequestAssociatedWithDedicatedPipe(&embedded_worker_host_ptr);
    return std::make_unique<ServiceWorkerContextClient>(
        1 /* embeded_worker_id */, 1 /* service_worker_version_id */,
        GURL("https://example.com") /* scope */,
        GURL("https://example.com/SW.js") /* script_URL */,
        false /* is_script_streaming */, std::move(event_dispatcher_request),
        std::move(controller_request), sw_host_ptr.PassInterface(),
        embedded_worker_host_ptr.PassInterface(),
        CreateProviderInfo(&out_pipes->registration_host_request,
                           &out_pipes->registration),
        nullptr /* embedded_worker_client */,
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        io_task_runner());
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner() const {
    return task_runner_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() {
    // Use this thread as the IO thread.
    return task_runner_;
  }

  base::MessageLoop message_loop_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ServiceWorkerContextClientTest, Ping) {
  ContextClientPipes pipes;
  std::unique_ptr<ServiceWorkerContextClient> context_client =
      CreateContextClient(&pipes);
  MockWebServiceWorkerContextProxy mock_proxy;
  context_client->WorkerContextStarted(&mock_proxy);

  bool is_called = false;
  pipes.event_dispatcher->Ping(CreateCallbackWithCalledFlag(&is_called));
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(is_called);
}

TEST_F(ServiceWorkerContextClientTest, DispatchFetchEvent) {
  ContextClientPipes pipes;
  MockWebServiceWorkerContextProxy mock_proxy;
  std::unique_ptr<ServiceWorkerContextClient> context_client;
  context_client = CreateContextClient(&pipes);
  context_client->WorkerContextStarted(&mock_proxy);
  context_client->DidEvaluateClassicScript(true /* success */);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  const GURL expected_url("https://example.com/expected");
  mojom::ServiceWorkerFetchResponseCallbackRequest fetch_callback_request;
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = expected_url;
  mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
  fetch_callback_request = mojo::MakeRequest(&fetch_callback_ptr);
  auto params = mojom::DispatchFetchEventParams::New();
  params->request = *request;
  pipes.event_dispatcher->DispatchFetchEvent(
      std::move(params), std::move(fetch_callback_ptr),
      base::BindOnce(
          [](blink::mojom::ServiceWorkerEventStatus, base::Time) {}));
  task_runner()->RunUntilIdle();

  ASSERT_EQ(1u, mock_proxy.fetch_events().size());
  EXPECT_EQ(request->url,
            static_cast<GURL>(mock_proxy.fetch_events()[0].second.Url()));
}

TEST_F(ServiceWorkerContextClientTest,
       DispatchOrQueueFetchEvent_NotRequestedTermination) {
  EnableServicification();
  ContextClientPipes pipes;
  std::unique_ptr<ServiceWorkerContextClient> context_client =
      CreateContextClient(&pipes);
  MockWebServiceWorkerContextProxy mock_proxy;
  context_client->WorkerContextStarted(&mock_proxy);
  context_client->DidEvaluateClassicScript(true /* success */);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  bool is_idle = false;
  auto timer = std::make_unique<ServiceWorkerTimeoutTimer>(
      CreateCallbackWithCalledFlag(&is_idle),
      task_runner()->DeprecatedGetMockTickClock());
  context_client->SetTimeoutTimerForTesting(std::move(timer));

  // The dispatched fetch event should be recorded by |mock_proxy|.
  const GURL expected_url("https://example.com/expected");
  mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
  mojom::ServiceWorkerFetchResponseCallbackRequest fetch_callback_request =
      mojo::MakeRequest(&fetch_callback_ptr);
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = expected_url;
  auto params = mojom::DispatchFetchEventParams::New();
  params->request = *request;
  context_client->DispatchOrQueueFetchEvent(
      std::move(params), std::move(fetch_callback_ptr),
      base::BindOnce(
          [](blink::mojom::ServiceWorkerEventStatus, base::Time) {}));
  task_runner()->RunUntilIdle();

  EXPECT_FALSE(context_client->RequestedTermination());
  ASSERT_EQ(1u, mock_proxy.fetch_events().size());
  EXPECT_EQ(expected_url,
            static_cast<GURL>(mock_proxy.fetch_events()[0].second.Url()));
}

TEST_F(ServiceWorkerContextClientTest,
       DispatchOrQueueFetchEvent_RequestedTerminationAndDie) {
  EnableServicification();
  ContextClientPipes pipes;
  std::unique_ptr<ServiceWorkerContextClient> context_client =
      CreateContextClient(&pipes);
  MockWebServiceWorkerContextProxy mock_proxy;
  context_client->WorkerContextStarted(&mock_proxy);
  context_client->DidEvaluateClassicScript(true /* success */);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  bool is_idle = false;
  auto timer = std::make_unique<ServiceWorkerTimeoutTimer>(
      CreateCallbackWithCalledFlag(&is_idle),
      task_runner()->DeprecatedGetMockTickClock());
  context_client->SetTimeoutTimerForTesting(std::move(timer));

  // Ensure the idle state.
  EXPECT_FALSE(context_client->RequestedTermination());
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kIdleDelay +
                               ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(context_client->RequestedTermination());

  const GURL expected_url("https://example.com/expected");
  mojom::ServiceWorkerFetchResponseCallbackRequest fetch_callback_request;

  // FetchEvent dispatched directly from the controlled clients through
  // mojom::ControllerServiceWorker should be queued in the idle state.
  {
    mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
    fetch_callback_request = mojo::MakeRequest(&fetch_callback_ptr);
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = expected_url;
    auto params = mojom::DispatchFetchEventParams::New();
    params->request = *request;
    pipes.controller->DispatchFetchEvent(
        std::move(params), std::move(fetch_callback_ptr),
        base::BindOnce(
            [](blink::mojom::ServiceWorkerEventStatus, base::Time) {}));
    task_runner()->RunUntilIdle();
  }
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  // Destruction of |context_client| should not hit any DCHECKs.
  context_client.reset();
}

TEST_F(ServiceWorkerContextClientTest,
       DispatchOrQueueFetchEvent_RequestedTerminationAndWakeUp) {
  EnableServicification();
  ContextClientPipes pipes;
  std::unique_ptr<ServiceWorkerContextClient> context_client =
      CreateContextClient(&pipes);
  MockWebServiceWorkerContextProxy mock_proxy;
  context_client->WorkerContextStarted(&mock_proxy);
  context_client->DidEvaluateClassicScript(true /* success */);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(mock_proxy.fetch_events().empty());
  bool is_idle = false;
  auto timer = std::make_unique<ServiceWorkerTimeoutTimer>(
      CreateCallbackWithCalledFlag(&is_idle),
      task_runner()->DeprecatedGetMockTickClock());
  context_client->SetTimeoutTimerForTesting(std::move(timer));

  // Ensure the idle state.
  EXPECT_FALSE(context_client->RequestedTermination());
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kIdleDelay +
                               ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(context_client->RequestedTermination());

  const GURL expected_url_1("https://example.com/expected_1");
  const GURL expected_url_2("https://example.com/expected_2");
  mojom::ServiceWorkerFetchResponseCallbackRequest fetch_callback_request_1;
  mojom::ServiceWorkerFetchResponseCallbackRequest fetch_callback_request_2;

  // FetchEvent dispatched directly from the controlled clients through
  // mojom::ControllerServiceWorker should be queued in the idle state.
  {
    mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
    fetch_callback_request_1 = mojo::MakeRequest(&fetch_callback_ptr);
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = expected_url_1;
    auto params = mojom::DispatchFetchEventParams::New();
    params->request = *request;
    pipes.controller->DispatchFetchEvent(
        std::move(params), std::move(fetch_callback_ptr),
        base::BindOnce(
            [](blink::mojom::ServiceWorkerEventStatus, base::Time) {}));
    task_runner()->RunUntilIdle();
  }
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  // Another event dispatched to mojom::ServiceWorkerEventDispatcher wakes up
  // the context client.
  {
    mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
    fetch_callback_request_2 = mojo::MakeRequest(&fetch_callback_ptr);
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = expected_url_2;
    auto params = mojom::DispatchFetchEventParams::New();
    params->request = *request;
    pipes.event_dispatcher->DispatchFetchEvent(
        std::move(params), std::move(fetch_callback_ptr),
        base::BindOnce(
            [](blink::mojom::ServiceWorkerEventStatus, base::Time) {}));
    task_runner()->RunUntilIdle();
  }
  EXPECT_FALSE(context_client->RequestedTermination());

  // All events should fire. The order of events should be kept.
  ASSERT_EQ(2u, mock_proxy.fetch_events().size());
  EXPECT_EQ(expected_url_1,
            static_cast<GURL>(mock_proxy.fetch_events()[0].second.Url()));
  EXPECT_EQ(expected_url_2,
            static_cast<GURL>(mock_proxy.fetch_events()[1].second.Url()));
}

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_state.mojom.h"

namespace content {
namespace service_worker_handle_unittest {

void VerifyStateChangedMessage(int expected_handle_id,
                               blink::mojom::ServiceWorkerState expected_state,
                               const IPC::Message* message) {
  ASSERT_TRUE(message != nullptr);
  ServiceWorkerMsg_ServiceWorkerStateChanged::Param param;
  ASSERT_TRUE(ServiceWorkerMsg_ServiceWorkerStateChanged::Read(
      message, &param));
  EXPECT_EQ(expected_handle_id, std::get<1>(param));
  EXPECT_EQ(expected_state, std::get<2>(param));
}

static void SaveStatusCallback(bool* called,
                               ServiceWorkerStatusCode* out,
                               ServiceWorkerStatusCode status) {
  *called = true;
  *out = status;
}

void SetUpDummyMessagePort(std::vector<blink::MessagePortChannel>* ports) {
  // Let the other end of the pipe close.
  mojo::MessagePipe pipe;
  ports->push_back(blink::MessagePortChannel(std::move(pipe.handle0)));
}

// A helper that holds on to ExtendableMessageEventPtr so it doesn't get
// destroyed after the message event handler runs.
class ExtendableMessageEventTestHelper : public EmbeddedWorkerTestHelper {
 public:
  ExtendableMessageEventTestHelper()
      : EmbeddedWorkerTestHelper(base::FilePath()) {}

  void OnExtendableMessageEvent(
      mojom::ExtendableMessageEventPtr event,
      mojom::ServiceWorkerEventDispatcher::
          DispatchExtendableMessageEventCallback callback) override {
    events_.push_back(std::move(event));
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::Time::Now());
  }

  const std::vector<mojom::ExtendableMessageEventPtr>& events() {
    return events_;
  }

 private:
  std::vector<mojom::ExtendableMessageEventPtr> events_;
};

class FailToStartWorkerTestHelper : public ExtendableMessageEventTestHelper {
 public:
  FailToStartWorkerTestHelper() : ExtendableMessageEventTestHelper() {}

  void OnStartWorker(
      int embedded_worker_id,
      int64_t service_worker_version_id,
      const GURL& scope,
      const GURL& script_url,
      bool pause_after_download,
      mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
      mojom::ControllerServiceWorkerRequest controller_request,
      blink::mojom::ServiceWorkerHostAssociatedPtrInfo service_worker_host,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
      blink::mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info)
      override {
    mojom::EmbeddedWorkerInstanceHostAssociatedPtr instance_host_ptr;
    instance_host_ptr.Bind(std::move(instance_host));
    instance_host_ptr->OnStopped();
    base::RunLoop().RunUntilIdle();
  }
};

class TestingServiceWorkerDispatcherHost : public ServiceWorkerDispatcherHost {
 public:
  TestingServiceWorkerDispatcherHost(int process_id,
                                     ResourceContext* resource_context,
                                     EmbeddedWorkerTestHelper* helper)
      : ServiceWorkerDispatcherHost(process_id, resource_context),
        bad_message_received_count_(0),
        helper_(helper) {}

  bool Send(IPC::Message* message) override { return helper_->Send(message); }

  void ShutdownForBadMessage() override { ++bad_message_received_count_; }

  int bad_message_received_count_;

 protected:
  EmbeddedWorkerTestHelper* helper_;
  ~TestingServiceWorkerDispatcherHost() override {}
};

class ServiceWorkerHandleTest : public testing::Test {
 public:
  ServiceWorkerHandleTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void Initialize(std::unique_ptr<EmbeddedWorkerTestHelper> helper) {
    helper_ = std::move(helper);

    dispatcher_host_ = new TestingServiceWorkerDispatcherHost(
        helper_->mock_render_process_id(), &resource_context_, helper_.get());
    helper_->RegisterDispatcherHost(helper_->mock_render_process_id(),
                                    dispatcher_host_);
    dispatcher_host_->Init(helper_->context_wrapper());
  }

  void SetUpRegistration(const GURL& scope, const GURL& script_url) {
    helper_->context()->storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();

    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    registration_ = new ServiceWorkerRegistration(
        options, helper_->context()->storage()->NewRegistrationId(),
        helper_->context()->AsWeakPtr());
    version_ =
        new ServiceWorkerVersion(registration_.get(), script_url,
                                 helper_->context()->storage()->NewVersionId(),
                                 helper_->context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(
        ServiceWorkerDatabase::ResourceRecord(10, version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);
    version_->SetMainScriptHttpResponseInfo(net::HttpResponseInfo());
    version_->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    version_->SetStatus(ServiceWorkerVersion::INSTALLING);

    // Make the registration findable via storage functions.
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    helper_->context()->storage()->StoreRegistration(
        registration_.get(),
        version_.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(SERVICE_WORKER_OK, status);
  }

  void TearDown() override {
    dispatcher_host_ = nullptr;
    registration_ = nullptr;
    version_ = nullptr;
    helper_.reset();
  }

  void CallDispatchExtendableMessageEvent(
      ServiceWorkerHandle* handle,
      ::blink::TransferableMessage message,
      base::OnceCallback<void(ServiceWorkerStatusCode)> callback) {
    handle->DispatchExtendableMessageEvent(std::move(message),
                                           std::move(callback));
  }

  size_t GetBindingsCount(ServiceWorkerHandle* handle) {
    return handle->bindings_.size();
  }

  ServiceWorkerHandle* GetServiceWorkerHandle(
      ServiceWorkerProviderHost* provider_host,
      int64_t version_id) {
    auto iter = provider_host->handles_.find(version_id);
    if (iter != provider_host->handles_.end())
      return iter->second.get();
    return nullptr;
  }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  TestBrowserThreadBundle browser_thread_bundle_;
  MockResourceContext resource_context_;

  base::SimpleTestTickClock tick_clock_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandleTest);
};

TEST_F(ServiceWorkerHandleTest, OnVersionStateChanged) {
  const int64_t kProviderId = 99;
  const int kRenderFrameId = 44;
  const GURL pattern("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));
  SetUpRegistration(pattern, script_url);

  ServiceWorkerRemoteProviderEndpoint remote_endpoint;
  std::unique_ptr<ServiceWorkerProviderHost> provider_host =
      CreateProviderHostWithDispatcherHost(
          helper_->mock_render_process_id(), kProviderId,
          helper_->context()->AsWeakPtr(), kRenderFrameId,
          dispatcher_host_.get(), &remote_endpoint);
  blink::mojom::ServiceWorkerObjectInfoPtr info =
      provider_host->GetOrCreateServiceWorkerHandle(version_.get());
  ServiceWorkerHandle* handle =
      GetServiceWorkerHandle(provider_host.get(), version_->version_id());

  // Start the worker, and then...
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // ...update state to installed.
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);

  ASSERT_EQ(0L, dispatcher_host_->bad_message_received_count_);

  const IPC::Message* message = nullptr;
  // StartWorker shouldn't be recorded here.
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  message = ipc_sink()->GetMessageAt(0);

  // StateChanged (state == Installed).
  VerifyStateChangedMessage(handle->handle_id(),
                            blink::mojom::ServiceWorkerState::kInstalled,
                            message);
}

TEST_F(ServiceWorkerHandleTest,
       DispatchExtendableMessageEvent_FromServiceWorker) {
  const GURL pattern("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<ExtendableMessageEventTestHelper>());
  SetUpRegistration(pattern, script_url);

  // Set mock clock on version_ to check timeout behavior.
  tick_clock_.SetNowTicks(base::TimeTicks::Now());
  version_->SetTickClockForTesting(&tick_clock_);

  // Make sure worker has a non-zero timeout.
  bool called = false;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        base::BindOnce(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::ACTIVATE, base::DoNothing(),
      base::TimeDelta::FromSeconds(10), ServiceWorkerVersion::KILL_ON_TIMEOUT);

  // Advance clock by a couple seconds.
  tick_clock_.Advance(base::TimeDelta::FromSeconds(4));
  base::TimeDelta remaining_time = version_->remaining_timeout();
  EXPECT_EQ(base::TimeDelta::FromSeconds(6), remaining_time);

  // Prepare a ServiceWorkerHandle corresponding to a JavaScript ServiceWorker
  // object in the service worker execution context for |version_|.
  ServiceWorkerProviderHost* provider_host = version_->provider_host();
  blink::mojom::ServiceWorkerObjectInfoPtr info =
      provider_host->GetOrCreateServiceWorkerHandle(version_.get());
  ServiceWorkerHandle* sender_worker_handle =
      GetServiceWorkerHandle(provider_host, version_->version_id());
  EXPECT_EQ(1u, GetBindingsCount(sender_worker_handle));

  // Dispatch an ExtendableMessageEvent simulating calling
  // ServiceWorker#postMessage() on the ServiceWorker object corresponding to
  // |service_worker_handle|.
  blink::TransferableMessage message;
  SetUpDummyMessagePort(&message.ports);
  called = false;
  status = SERVICE_WORKER_ERROR_MAX_VALUE;
  CallDispatchExtendableMessageEvent(
      sender_worker_handle, std::move(message),
      base::BindOnce(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  // The dispatched ExtendableMessageEvent should be kept in
  // ExtendableMessageEventTestHelper, and the source service worker object info
  // should correspond to the pair (|version_->provider_host()|, |version_|),
  // means it should correspond to |sender_worker_handle|.
  EXPECT_EQ(2u, GetBindingsCount(sender_worker_handle));
  const std::vector<mojom::ExtendableMessageEventPtr>& events =
      static_cast<ExtendableMessageEventTestHelper*>(helper_.get())->events();
  EXPECT_EQ(1u, events.size());
  EXPECT_FALSE(events[0]->source_info_for_client);
  EXPECT_TRUE(events[0]->source_info_for_service_worker);
  EXPECT_EQ(sender_worker_handle->handle_id(),
            events[0]->source_info_for_service_worker->handle_id);
  EXPECT_EQ(version_->version_id(),
            events[0]->source_info_for_service_worker->version_id);

  // Timeout of message event should not have extended life of service worker.
  EXPECT_EQ(remaining_time, version_->remaining_timeout());
}

TEST_F(ServiceWorkerHandleTest, DispatchExtendableMessageEvent_FromClient) {
  const int64_t kProviderId = 99;
  const GURL pattern("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<ExtendableMessageEventTestHelper>());
  SetUpRegistration(pattern, script_url);

  // Prepare a ServiceWorkerProviderHost for a window client. A
  // WebContents/RenderFrameHost must be created too because it's needed for
  // DispatchExtendableMessageEvent to populate ExtendableMessageEvent#source.
  RenderViewHostTestEnabler rvh_test_enabler;
  std::unique_ptr<WebContents> web_contents(
      WebContentsTester::CreateTestWebContents(helper_->browser_context(),
                                               nullptr));
  RenderFrameHost* frame_host = web_contents->GetMainFrame();
  ServiceWorkerProviderHostInfo provider_host_info(
      kProviderId, frame_host->GetRoutingID(),
      blink::mojom::ServiceWorkerProviderType::kForWindow,
      true /* is_parent_frame_secure */);
  std::unique_ptr<ServiceWorkerProviderHost> provider_host =
      ServiceWorkerProviderHost::Create(
          frame_host->GetProcess()->GetID(), std::move(provider_host_info),
          helper_->context()->AsWeakPtr(), dispatcher_host_->AsWeakPtr());
  provider_host->SetDocumentUrl(pattern);
  // Prepare a ServiceWorkerHandle for the above |provider_host|.
  blink::mojom::ServiceWorkerObjectInfoPtr info =
      provider_host->GetOrCreateServiceWorkerHandle(version_.get());
  ServiceWorkerHandle* handle =
      GetServiceWorkerHandle(provider_host.get(), version_->version_id());

  // Simulate dispatching an ExtendableMessageEvent.
  blink::TransferableMessage message;
  SetUpDummyMessagePort(&message.ports);
  bool called = false;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  CallDispatchExtendableMessageEvent(
      handle, std::move(message),
      base::BindOnce(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  // The dispatched ExtendableMessageEvent should be kept in
  // ExtendableMessageEventTestHelper, and its source client info should
  // correspond to |provider_host|.
  const std::vector<mojom::ExtendableMessageEventPtr>& events =
      static_cast<ExtendableMessageEventTestHelper*>(helper_.get())->events();
  EXPECT_EQ(1u, events.size());
  EXPECT_FALSE(events[0]->source_info_for_service_worker);
  EXPECT_TRUE(events[0]->source_info_for_client);
  EXPECT_EQ(provider_host->client_uuid(),
            events[0]->source_info_for_client->client_uuid);
  EXPECT_EQ(provider_host->client_type(),
            events[0]->source_info_for_client->client_type);
}

TEST_F(ServiceWorkerHandleTest, DispatchExtendableMessageEvent_Fail) {
  const int64_t kProviderId = 99;
  const GURL pattern("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<FailToStartWorkerTestHelper>());
  SetUpRegistration(pattern, script_url);

  // Prepare a ServiceWorkerProviderHost for a window client. A
  // WebContents/RenderFrameHost must be created too because it's needed for
  // DispatchExtendableMessageEvent to populate ExtendableMessageEvent#source.
  RenderViewHostTestEnabler rvh_test_enabler;
  std::unique_ptr<WebContents> web_contents(
      WebContentsTester::CreateTestWebContents(helper_->browser_context(),
                                               nullptr));
  RenderFrameHost* frame_host = web_contents->GetMainFrame();
  ServiceWorkerProviderHostInfo provider_host_info(
      kProviderId, frame_host->GetRoutingID(),
      blink::mojom::ServiceWorkerProviderType::kForWindow,
      true /* is_parent_frame_secure */);
  std::unique_ptr<ServiceWorkerProviderHost> provider_host =
      ServiceWorkerProviderHost::Create(
          frame_host->GetProcess()->GetID(), std::move(provider_host_info),
          helper_->context()->AsWeakPtr(), dispatcher_host_->AsWeakPtr());
  provider_host->SetDocumentUrl(pattern);
  // Prepare a ServiceWorkerHandle for the above |provider_host|.
  blink::mojom::ServiceWorkerObjectInfoPtr info =
      provider_host->GetOrCreateServiceWorkerHandle(version_.get());
  ServiceWorkerHandle* handle =
      GetServiceWorkerHandle(provider_host.get(), version_->version_id());

  // Try to dispatch ExtendableMessageEvent. This should fail to start the
  // worker and to dispatch the event.
  blink::TransferableMessage message;
  SetUpDummyMessagePort(&message.ports);
  bool called = false;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  CallDispatchExtendableMessageEvent(
      handle, std::move(message),
      base::BindOnce(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
  // No ExtendableMessageEvent has been dispatched.
  const std::vector<mojom::ExtendableMessageEventPtr>& events =
      static_cast<ExtendableMessageEventTestHelper*>(helper_.get())->events();
  EXPECT_EQ(0u, events.size());
}

}  // namespace service_worker_handle_unittest
}  // namespace content

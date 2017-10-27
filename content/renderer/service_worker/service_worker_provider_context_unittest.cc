// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_provider_context.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/renderer/child_url_loader_factory_getter.h"
#include "content/renderer/service_worker/service_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_handle_reference.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/web_service_worker_impl.h"
#include "content/renderer/service_worker/web_service_worker_registration_impl.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

namespace {

class MockServiceWorkerRegistrationObjectHost
    : public blink::mojom::ServiceWorkerRegistrationObjectHost {
 public:
  MockServiceWorkerRegistrationObjectHost() = default;
  ~MockServiceWorkerRegistrationObjectHost() override = default;

  void AddBinding(
      blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedRequest
          request) {
    bindings_.AddBinding(this, std::move(request));
  }

  int GetBindingCount() const { return bindings_.size(); }

 private:
  // Implements blink::mojom::ServiceWorkerRegistrationObjectHost.
  void Update(UpdateCallback callback) override {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                            base::nullopt);
  }
  void Unregister(UnregisterCallback callback) override {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                            base::nullopt);
  }
  void EnableNavigationPreload(
      bool enable,
      EnableNavigationPreloadCallback callback) override {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                            base::nullopt);
  }

  mojo::AssociatedBindingSet<blink::mojom::ServiceWorkerRegistrationObjectHost>
      bindings_;
};

class MockWebServiceWorkerProviderClientImpl
    : public blink::WebServiceWorkerProviderClient {
 public:
  MockWebServiceWorkerProviderClientImpl() {}

  ~MockWebServiceWorkerProviderClientImpl() override {}

  void SetController(std::unique_ptr<blink::WebServiceWorker::Handle> handle,
                     bool should_notify_controller_change) override {
    was_set_controller_called_ = true;
  }

  void DispatchMessageEvent(
      std::unique_ptr<blink::WebServiceWorker::Handle> handle,
      const blink::WebString& message,
      blink::WebVector<blink::MessagePortChannel> ports) override {
    was_dispatch_message_event_called_ = true;
  }

  void CountFeature(uint32_t feature) override {
    used_features_.insert(feature);
  }

  bool was_set_controller_called() const { return was_set_controller_called_; }

  bool was_dispatch_message_event_called() const {
    return was_dispatch_message_event_called_;
  }

 private:
  bool was_set_controller_called_ = false;
  bool was_dispatch_message_event_called_ = false;
  std::set<uint32_t> used_features_;
};

class ServiceWorkerTestSender : public ThreadSafeSender {
 public:
  explicit ServiceWorkerTestSender(IPC::TestSink* ipc_sink)
      : ThreadSafeSender(nullptr, nullptr), ipc_sink_(ipc_sink) {}

  bool Send(IPC::Message* message) override { return ipc_sink_->Send(message); }

 private:
  ~ServiceWorkerTestSender() override {}

  IPC::TestSink* ipc_sink_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTestSender);
};

}  // namespace

class ServiceWorkerProviderContextTest : public testing::Test {
 public:
  ServiceWorkerProviderContextTest() = default;

  void SetUp() override {
    sender_ = new ServiceWorkerTestSender(&ipc_sink_);
    dispatcher_.reset(new ServiceWorkerDispatcher(sender_.get(), nullptr));
  }

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  CreateServiceWorkerRegistrationObjectInfo() {
    auto info = blink::mojom::ServiceWorkerRegistrationObjectInfo::New();
    info->handle_id = 10;
    info->registration_id = 20;
    remote_registration_object_host_.AddBinding(
        mojo::MakeRequest(&info->host_ptr_info));

    info->active = blink::mojom::ServiceWorkerObjectInfo::New();
    info->active->handle_id = 100;
    info->active->version_id = 200;
    info->waiting = blink::mojom::ServiceWorkerObjectInfo::New();
    info->waiting->handle_id = 101;
    info->waiting->version_id = 201;
    info->installing = blink::mojom::ServiceWorkerObjectInfo::New();
    info->installing->handle_id = 102;
    info->installing->version_id = 202;
    return info;
  }

  ThreadSafeSender* thread_safe_sender() { return sender_.get(); }
  IPC::TestSink* ipc_sink() { return &ipc_sink_; }
  ServiceWorkerDispatcher* dispatcher() { return dispatcher_.get(); }
  const MockServiceWorkerRegistrationObjectHost&
  remote_registration_object_host() const {
    return remote_registration_object_host_;
  }

 private:
  base::MessageLoop message_loop_;
  IPC::TestSink ipc_sink_;
  std::unique_ptr<ServiceWorkerDispatcher> dispatcher_;
  scoped_refptr<ServiceWorkerTestSender> sender_;
  MockServiceWorkerRegistrationObjectHost remote_registration_object_host_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderContextTest);
};

TEST_F(ServiceWorkerProviderContextTest, CreateForController) {
  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info =
      CreateServiceWorkerRegistrationObjectInfo();
  // ServiceWorkerRegistrationObjectHost Mojo connection has been added.
  ASSERT_EQ(1, remote_registration_object_host().GetBindingCount());

  // Set up ServiceWorkerProviderContext for ServiceWorkerGlobalScope.
  const int kProviderId = 10;
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, SERVICE_WORKER_PROVIDER_FOR_CONTROLLER, nullptr, nullptr,
      dispatcher(), nullptr /* loader_factory_getter */);

  // The passed references should be adopted and owned by the provider context.
  provider_context->SetRegistrationForServiceWorkerGlobalScope(
      std::move(registration_info), thread_safe_sender());
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Destruction of the provider context should release references to the
  // associated registration and its versions.
  provider_context = nullptr;
  ASSERT_EQ(3UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  // ServiceWorkerRegistrationObjectHost Mojo connection got broken.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, remote_registration_object_host().GetBindingCount());
}

TEST_F(ServiceWorkerProviderContextTest, SetController) {
  const int kProviderId = 10;

  {
    // Assume that these objects are passed from the browser process and own
    // references to browser-side registration/worker representations.
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info =
        CreateServiceWorkerRegistrationObjectInfo();

    // (1) In the case there is no WebSWProviderClient but SWProviderContext for
    // the provider, the passed reference should be adopted and owned by the
    // provider context.
    mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
    mojom::ServiceWorkerContainerAssociatedRequest container_request =
        mojo::MakeIsolatedRequest(&container_ptr);
    auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
        kProviderId, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
        std::move(container_request), nullptr /* host_ptr_info */, dispatcher(),
        nullptr /* loader_factory_getter */);

    ipc_sink()->ClearMessages();
    container_ptr->SetController(std::move(registration_info->active),
                                 std::vector<blink::mojom::WebFeature>(), true);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(0UL, ipc_sink()->message_count());

    // Destruction of the provider context should release references to the
    // the controller.
    provider_context = nullptr;
    ASSERT_EQ(1UL, ipc_sink()->message_count());
    EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
              ipc_sink()->GetMessageAt(0)->type());
    ipc_sink()->ClearMessages();
  }

  {
    // Assume that these objects are passed from the browser process and own
    // references to browser-side registration/worker representations.
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info =
        CreateServiceWorkerRegistrationObjectInfo();

    // (2) In the case there are both SWProviderContext and SWProviderClient for
    // the provider, the passed reference should be adopted and owned by the
    // provider context. In addition, the new reference should be created for
    // the provider client and immediately released due to limitation of the
    // mock implementation.
    mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
    mojom::ServiceWorkerContainerHostAssociatedRequest host_request =
        mojo::MakeRequest(&host_ptr_info);

    mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
    mojom::ServiceWorkerContainerAssociatedRequest container_request =
        mojo::MakeIsolatedRequest(&container_ptr);
    auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
        kProviderId, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
        std::move(container_request), std::move(host_ptr_info), dispatcher(),
        nullptr /* loader_factory_getter */);
    auto provider_impl = std::make_unique<WebServiceWorkerProviderImpl>(
        thread_safe_sender(), provider_context.get());
    auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
    provider_impl->SetClient(client.get());
    ASSERT_FALSE(client->was_set_controller_called());

    ipc_sink()->ClearMessages();
    container_ptr->SetController(std::move(registration_info->active),
                                 std::vector<blink::mojom::WebFeature>(), true);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(client->was_set_controller_called());
    ASSERT_EQ(2UL, ipc_sink()->message_count());
    EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
              ipc_sink()->GetMessageAt(0)->type());
    EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
              ipc_sink()->GetMessageAt(1)->type());
  }
}

// Test that clearing the controller by sending a kInvalidServiceWorkerHandle
// results in the provider context having a null controller.
TEST_F(ServiceWorkerProviderContextTest, SetController_Null) {
  const int kProviderId = 10;

  mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
  mojom::ServiceWorkerContainerHostAssociatedRequest host_request =
      mojo::MakeRequest(&host_ptr_info);

  mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest container_request =
      mojo::MakeIsolatedRequest(&container_ptr);
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      std::move(container_request), std::move(host_ptr_info), dispatcher(),
      nullptr /* loader_factory_getter */);
  auto provider_impl = std::make_unique<WebServiceWorkerProviderImpl>(
      thread_safe_sender(), provider_context.get());
  auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
  provider_impl->SetClient(client.get());

  container_ptr->SetController(blink::mojom::ServiceWorkerObjectInfo::New(),
                               std::vector<blink::mojom::WebFeature>(), true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nullptr, provider_context->controller());
  EXPECT_TRUE(client->was_set_controller_called());
}

}  // namespace content

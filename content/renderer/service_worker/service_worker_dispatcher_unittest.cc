// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_dispatcher.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/renderer/service_worker/service_worker_handle_reference.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/web_service_worker_impl.h"
#include "content/renderer/service_worker/web_service_worker_registration_impl.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

namespace {

class MockServiceWorkerRegistrationObjectHost
    : public blink::mojom::ServiceWorkerRegistrationObjectHost {
 public:
  MockServiceWorkerRegistrationObjectHost() {
    bindings_.set_connection_error_handler(
        base::Bind(&MockServiceWorkerRegistrationObjectHost::OnConnectionError,
                   base::Unretained(this)));
  }
  ~MockServiceWorkerRegistrationObjectHost() override = default;

  void AddBinding(
      blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedRequest
          request) {
    bindings_.AddBinding(this, std::move(request));
  }

  blink::mojom::ServiceWorkerRegistrationObjectAssociatedRequest
  CreateRegistrationObjectRequest() {
    if (!remote_registration_)
      return mojo::MakeRequest(&remote_registration_);
    return nullptr;
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
  void GetNavigationPreloadState(
      GetNavigationPreloadStateCallback callback) override {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                            base::nullopt, nullptr);
  }
  void SetNavigationPreloadHeader(
      const std::string& value,
      SetNavigationPreloadHeaderCallback callback) override {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                            base::nullopt);
  }

  void OnConnectionError() {
    // If there are still bindings, |this| is still being used.
    if (!bindings_.empty())
      return;
    // Will destroy corresponding remote WebServiceWorkerRegistrationImpl
    // instance.
    remote_registration_.reset();
  }

  mojo::AssociatedBindingSet<blink::mojom::ServiceWorkerRegistrationObjectHost>
      bindings_;
  blink::mojom::ServiceWorkerRegistrationObjectAssociatedPtr
      remote_registration_;
};

class ServiceWorkerTestSender : public ThreadSafeSender {
 public:
  explicit ServiceWorkerTestSender(IPC::TestSink* ipc_sink)
      : ThreadSafeSender(nullptr, nullptr),
        ipc_sink_(ipc_sink) {}

  bool Send(IPC::Message* message) override {
    return ipc_sink_->Send(message);
  }

 private:
  ~ServiceWorkerTestSender() override {}

  IPC::TestSink* ipc_sink_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTestSender);
};

}  // namespace

class ServiceWorkerDispatcherTest : public testing::Test {
 public:
  ServiceWorkerDispatcherTest() {}

  void SetUp() override {
    sender_ = new ServiceWorkerTestSender(&ipc_sink_);
    dispatcher_.reset(new ServiceWorkerDispatcher(sender_.get(), nullptr));
  }

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  CreateServiceWorkerRegistrationObjectInfo() {
    auto info = blink::mojom::ServiceWorkerRegistrationObjectInfo::New();
    info->registration_id = 20;
    remote_registration_object_host_.AddBinding(
        mojo::MakeRequest(&info->host_ptr_info));
    info->request =
        remote_registration_object_host_.CreateRegistrationObjectRequest();

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

  bool ContainsServiceWorker(int handle_id) {
    return ContainsKey(dispatcher_->service_workers_, handle_id);
  }

  std::unique_ptr<ServiceWorkerHandleReference> Adopt(
      blink::mojom::ServiceWorkerObjectInfoPtr info) {
    return ServiceWorkerHandleReference::Adopt(
        std::move(info), dispatcher_->thread_safe_sender());
  }

  ServiceWorkerDispatcher* dispatcher() { return dispatcher_.get(); }
  ThreadSafeSender* thread_safe_sender() { return sender_.get(); }
  IPC::TestSink* ipc_sink() { return &ipc_sink_; }

 private:
  base::MessageLoop message_loop_;
  IPC::TestSink ipc_sink_;
  std::unique_ptr<ServiceWorkerDispatcher> dispatcher_;
  scoped_refptr<ServiceWorkerTestSender> sender_;
  MockServiceWorkerRegistrationObjectHost remote_registration_object_host_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcherTest);
};

TEST_F(ServiceWorkerDispatcherTest, GetServiceWorker) {
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      CreateServiceWorkerRegistrationObjectInfo();

  // Should return a worker object newly created with the given reference.
  scoped_refptr<WebServiceWorkerImpl> worker(
      dispatcher()->GetOrCreateServiceWorker(Adopt(info->installing->Clone())));
  EXPECT_TRUE(worker);
  EXPECT_TRUE(ContainsServiceWorker(info->installing->handle_id));
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Should return the same worker object and release the given reference.
  scoped_refptr<WebServiceWorkerImpl> existing_worker =
      dispatcher()->GetOrCreateServiceWorker(
          Adopt(std::move(info->installing)));
  EXPECT_EQ(worker, existing_worker);
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  ipc_sink()->ClearMessages();

  // Should return nullptr when a given object is invalid.
  scoped_refptr<WebServiceWorkerImpl> invalid_worker =
      dispatcher()->GetOrCreateServiceWorker(
          Adopt(blink::mojom::ServiceWorkerObjectInfo::New()));
  EXPECT_FALSE(invalid_worker);
  EXPECT_EQ(0UL, ipc_sink()->message_count());
}

}  // namespace content

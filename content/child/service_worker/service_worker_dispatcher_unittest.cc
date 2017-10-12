// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_dispatcher.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
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
  mojo::AssociatedBindingSet<blink::mojom::ServiceWorkerRegistrationObjectHost>
      bindings_;
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

  void CreateObjectInfoAndVersionAttributes(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr* info,
      ServiceWorkerVersionAttributes* attrs) {
    *info = blink::mojom::ServiceWorkerRegistrationObjectInfo::New();
    (*info)->handle_id = 10;
    (*info)->registration_id = 20;
    remote_registration_object_host_.AddBinding(
        mojo::MakeRequest(&(*info)->host_ptr_info));

    attrs->active.handle_id = 100;
    attrs->active.version_id = 200;
    attrs->waiting.handle_id = 101;
    attrs->waiting.version_id = 201;
    attrs->installing.handle_id = 102;
    attrs->installing.version_id = 202;
  }

  bool ContainsServiceWorker(int handle_id) {
    return ContainsKey(dispatcher_->service_workers_, handle_id);
  }

  bool ContainsRegistration(int registration_handle_id) {
    return ContainsKey(dispatcher_->registrations_, registration_handle_id);
  }

  void OnPostMessage(const ServiceWorkerMsg_MessageToDocument_Params& params) {
    dispatcher_->OnPostMessage(params);
  }

  std::unique_ptr<ServiceWorkerHandleReference> Adopt(
      const ServiceWorkerObjectInfo& info) {
    return dispatcher_->Adopt(info);
  }

  ServiceWorkerDispatcher* dispatcher() { return dispatcher_.get(); }
  ThreadSafeSender* thread_safe_sender() { return sender_.get(); }
  IPC::TestSink* ipc_sink() { return &ipc_sink_; }
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

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcherTest);
};

class MockWebServiceWorkerProviderClientImpl
    : public blink::WebServiceWorkerProviderClient {
 public:
  MockWebServiceWorkerProviderClientImpl(int provider_id,
                                         ServiceWorkerDispatcher* dispatcher)
      : provider_id_(provider_id), dispatcher_(dispatcher) {
    dispatcher_->AddProviderClient(provider_id, this);
  }

  ~MockWebServiceWorkerProviderClientImpl() override {
    dispatcher_->RemoveProviderClient(provider_id_);
  }

  void SetController(std::unique_ptr<blink::WebServiceWorker::Handle> handle,
                     bool should_notify_controller_change) override {}

  void DispatchMessageEvent(
      std::unique_ptr<blink::WebServiceWorker::Handle> handle,
      const blink::WebString& message,
      blink::WebVector<blink::MessagePortChannel> ports) override {
    // WebPassOwnPtr cannot be owned in Chromium, so drop the handle here.
    // The destruction releases ServiceWorkerHandleReference.
    is_dispatch_message_event_called_ = true;
  }

  void CountFeature(uint32_t feature) override {
    used_features_.insert(feature);
  }

  bool is_dispatch_message_event_called() const {
    return is_dispatch_message_event_called_;
  }

 private:
  const int provider_id_;
  bool is_dispatch_message_event_called_ = false;
  ServiceWorkerDispatcher* dispatcher_;
  std::set<uint32_t> used_features_;
};

TEST_F(ServiceWorkerDispatcherTest, OnPostMessage) {
  const int kProviderId = 10;

  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  ServiceWorkerMsg_MessageToDocument_Params params;
  params.thread_id = kDocumentMainThreadId;
  params.provider_id = kProviderId;
  params.service_worker_info = attrs.active;

  // The passed reference should be adopted but immediately released because
  // there is no provider client.
  OnPostMessage(params);
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  ipc_sink()->ClearMessages();

  std::unique_ptr<MockWebServiceWorkerProviderClientImpl> provider_client(
      new MockWebServiceWorkerProviderClientImpl(kProviderId, dispatcher()));
  ASSERT_FALSE(provider_client->is_dispatch_message_event_called());

  // The passed reference should be owned by the provider client (but the
  // reference is immediately released due to limitation of the mock provider
  // client. See the comment on dispatchMessageEvent() of the mock).
  OnPostMessage(params);
  EXPECT_TRUE(provider_client->is_dispatch_message_event_called());
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
}

TEST_F(ServiceWorkerDispatcherTest, GetServiceWorker) {
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Should return a worker object newly created with the given reference.
  scoped_refptr<WebServiceWorkerImpl> worker(
      dispatcher()->GetOrCreateServiceWorker(Adopt(attrs.installing)));
  EXPECT_TRUE(worker);
  EXPECT_TRUE(ContainsServiceWorker(attrs.installing.handle_id));
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Should return the same worker object and release the given reference.
  scoped_refptr<WebServiceWorkerImpl> existing_worker =
      dispatcher()->GetOrCreateServiceWorker(Adopt(attrs.installing));
  EXPECT_EQ(worker, existing_worker);
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  ipc_sink()->ClearMessages();

  // Should return nullptr when a given object is invalid.
  scoped_refptr<WebServiceWorkerImpl> invalid_worker =
      dispatcher()->GetOrCreateServiceWorker(Adopt(ServiceWorkerObjectInfo()));
  EXPECT_FALSE(invalid_worker);
  EXPECT_EQ(0UL, ipc_sink()->message_count());
}

TEST_F(ServiceWorkerDispatcherTest, GetOrCreateRegistration) {
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration1;
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration2;

  {
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
    ServiceWorkerVersionAttributes attrs;
    CreateObjectInfoAndVersionAttributes(&info, &attrs);
    int64_t registration_id = info->registration_id;
    int32_t handle_id = info->handle_id;
    // The 1st ServiceWorkerRegistrationObjectHost Mojo connection has been
    // added.
    ASSERT_EQ(1, remote_registration_object_host().GetBindingCount());

    // Should return a registration object newly created with incrementing
    // the refcounts.
    registration1 =
        dispatcher()->GetOrCreateRegistration(std::move(info), attrs);
    EXPECT_TRUE(registration1);
    EXPECT_TRUE(ContainsRegistration(handle_id));
    EXPECT_EQ(registration_id, registration1->RegistrationId());
    EXPECT_EQ(1, remote_registration_object_host().GetBindingCount());
    ASSERT_EQ(3UL, ipc_sink()->message_count());
    EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
              ipc_sink()->GetMessageAt(0)->type());
    EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
              ipc_sink()->GetMessageAt(1)->type());
    EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
              ipc_sink()->GetMessageAt(2)->type());
  }

  ipc_sink()->ClearMessages();

  {
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
    ServiceWorkerVersionAttributes attrs;
    CreateObjectInfoAndVersionAttributes(&info, &attrs);
    // The 2nd Mojo connection has been added.
    ASSERT_EQ(2, remote_registration_object_host().GetBindingCount());
    // Should return the same registration object without incrementing the
    // refcounts.
    registration2 =
        dispatcher()->GetOrCreateRegistration(std::move(info), attrs);
    EXPECT_TRUE(registration2);
    EXPECT_EQ(registration1, registration2);
    // The 2nd Mojo connection has been dropped.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1, remote_registration_object_host().GetBindingCount());
    EXPECT_EQ(0UL, ipc_sink()->message_count());
  }

  ipc_sink()->ClearMessages();

  // The registration dtor decrements the refcounts.
  registration1 = nullptr;
  registration2 = nullptr;
  ASSERT_EQ(3UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  // The 1st Mojo connection has been dropped.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, remote_registration_object_host().GetBindingCount());
}

TEST_F(ServiceWorkerDispatcherTest, GetOrAdoptRegistration) {
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration1;
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration2;

  {
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
    ServiceWorkerVersionAttributes attrs;
    CreateObjectInfoAndVersionAttributes(&info, &attrs);
    int64_t registration_id = info->registration_id;
    int32_t handle_id = info->handle_id;
    // The 1st ServiceWorkerRegistrationObjectHost Mojo connection has been
    // added.
    ASSERT_EQ(1, remote_registration_object_host().GetBindingCount());

    // Should return a registration object newly created with adopting the
    // refcounts.
    registration1 =
        dispatcher()->GetOrAdoptRegistration(std::move(info), attrs);
    EXPECT_TRUE(registration1);
    EXPECT_TRUE(ContainsRegistration(handle_id));
    EXPECT_EQ(registration_id, registration1->RegistrationId());
    EXPECT_EQ(1, remote_registration_object_host().GetBindingCount());
    EXPECT_EQ(0UL, ipc_sink()->message_count());
  }

  ipc_sink()->ClearMessages();

  {
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
    ServiceWorkerVersionAttributes attrs;
    CreateObjectInfoAndVersionAttributes(&info, &attrs);
    // The 2nd Mojo connection has been added.
    ASSERT_EQ(2, remote_registration_object_host().GetBindingCount());
    // Should return the same registration object without incrementing the
    // refcounts.
    registration2 =
        dispatcher()->GetOrAdoptRegistration(std::move(info), attrs);
    EXPECT_TRUE(registration2);
    EXPECT_EQ(registration1, registration2);
    // The 2nd Mojo connection has been dropped.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1, remote_registration_object_host().GetBindingCount());
    ASSERT_EQ(3UL, ipc_sink()->message_count());
    EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
              ipc_sink()->GetMessageAt(0)->type());
    EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
              ipc_sink()->GetMessageAt(1)->type());
    EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
              ipc_sink()->GetMessageAt(2)->type());
  }

  ipc_sink()->ClearMessages();

  // The registration dtor decrements the refcounts.
  registration1 = nullptr;
  registration2 = nullptr;
  ASSERT_EQ(3UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  // The 1st Mojo connection has been dropped.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, remote_registration_object_host().GetBindingCount());
}

}  // namespace content

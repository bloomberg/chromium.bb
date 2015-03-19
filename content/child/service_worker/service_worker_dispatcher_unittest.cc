// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop_proxy.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

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

class ServiceWorkerDispatcherTest : public testing::Test {
 public:
  ServiceWorkerDispatcherTest() {}

  void SetUp() override {
    sender_ = new ServiceWorkerTestSender(&ipc_sink_);
    dispatcher_.reset(new ServiceWorkerDispatcher(sender_.get()));
  }

  void CreateObjectInfoAndVersionAttributes(
      ServiceWorkerRegistrationObjectInfo* info,
      ServiceWorkerVersionAttributes* attrs) {
    info->handle_id = 10;
    info->registration_id = 20;

    attrs->active.handle_id = 100;
    attrs->active.version_id = 200;
    attrs->waiting.handle_id = 101;
    attrs->waiting.version_id = 201;
    attrs->installing.handle_id = 102;
    attrs->installing.version_id = 202;
  }

  WebServiceWorkerRegistrationImpl* FindOrCreateRegistration(
      const ServiceWorkerRegistrationObjectInfo& info,
      const ServiceWorkerVersionAttributes& attrs) {
    return dispatcher_->FindOrCreateRegistration(info, attrs);
  }

  bool ContainsServiceWorker(int handle_id) {
    return ContainsKey(dispatcher_->service_workers_, handle_id);
  }

  bool ContainsRegistration(int registration_handle_id) {
    return ContainsKey(dispatcher_->registrations_, registration_handle_id);
  }

  ServiceWorkerDispatcher* dispatcher() { return dispatcher_.get(); }
  IPC::TestSink* ipc_sink() { return &ipc_sink_; }

 private:
  IPC::TestSink ipc_sink_;
  scoped_ptr<ServiceWorkerDispatcher> dispatcher_;
  scoped_refptr<ServiceWorkerTestSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcherTest);
};

// TODO(nhiroki): Add tests for message handlers especially to receive reference
// counts like OnAssociateRegistration().
TEST_F(ServiceWorkerDispatcherTest, GetServiceWorker) {
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Should return a worker object newly created with incrementing refcount.
  bool adopt_handle = false;
  scoped_ptr<WebServiceWorkerImpl> worker(
      dispatcher()->GetServiceWorker(attrs.installing, adopt_handle));
  EXPECT_TRUE(worker);
  EXPECT_TRUE(ContainsServiceWorker(attrs.installing.handle_id));
  EXPECT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());

  ipc_sink()->ClearMessages();

  // Should return the existing worker object.
  adopt_handle = false;
  WebServiceWorkerImpl* existing_worker =
      dispatcher()->GetServiceWorker(attrs.installing, adopt_handle);
  EXPECT_EQ(worker, existing_worker);
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Should return the existing worker object with adopting refcount.
  adopt_handle = true;
  existing_worker =
      dispatcher()->GetServiceWorker(attrs.installing, adopt_handle);
  EXPECT_EQ(worker, existing_worker);
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());

  ipc_sink()->ClearMessages();

  // Should return another worker object newly created with adopting refcount.
  adopt_handle = true;
  scoped_ptr<WebServiceWorkerImpl> another_worker(
      dispatcher()->GetServiceWorker(attrs.waiting, adopt_handle));
  EXPECT_NE(worker.get(), another_worker.get());
  EXPECT_TRUE(ContainsServiceWorker(attrs.waiting.handle_id));
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Should return nullptr when a given object info is invalid.
  adopt_handle = false;
  WebServiceWorkerImpl* invalid_worker =
      dispatcher()->GetServiceWorker(ServiceWorkerObjectInfo(), adopt_handle);
  EXPECT_FALSE(invalid_worker);
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  adopt_handle = true;
  invalid_worker =
      dispatcher()->GetServiceWorker(ServiceWorkerObjectInfo(), adopt_handle);
  EXPECT_FALSE(invalid_worker);
  EXPECT_EQ(0UL, ipc_sink()->message_count());
}

TEST_F(ServiceWorkerDispatcherTest, CreateServiceWorkerRegistration) {
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Should return a registration object newly created with incrementing
  // refcount.
  bool adopt_handle = false;
  scoped_ptr<WebServiceWorkerRegistrationImpl> registration(
      dispatcher()->CreateServiceWorkerRegistration(info, adopt_handle));
  EXPECT_TRUE(registration);
  EXPECT_TRUE(ContainsRegistration(info.handle_id));
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());

  registration.reset();
  EXPECT_FALSE(ContainsRegistration(info.handle_id));
  ipc_sink()->ClearMessages();

  // Should return another registration object newly created with adopting
  // refcount.
  adopt_handle = true;
  scoped_ptr<WebServiceWorkerRegistrationImpl> another_registration(
      dispatcher()->CreateServiceWorkerRegistration(info, adopt_handle));
  EXPECT_TRUE(another_registration);
  EXPECT_TRUE(ContainsRegistration(info.handle_id));
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  another_registration.reset();
  ipc_sink()->ClearMessages();

  // should return nullptr when a given object info is invalid.
  adopt_handle = false;
  scoped_ptr<WebServiceWorkerRegistrationImpl> invalid_registration(
      dispatcher()->CreateServiceWorkerRegistration(
          ServiceWorkerRegistrationObjectInfo(), adopt_handle));
  EXPECT_FALSE(invalid_registration);
  EXPECT_FALSE(ContainsRegistration(info.handle_id));
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  adopt_handle = true;
  invalid_registration.reset(dispatcher()->CreateServiceWorkerRegistration(
      ServiceWorkerRegistrationObjectInfo(), adopt_handle));
  EXPECT_FALSE(invalid_registration);
  EXPECT_FALSE(ContainsRegistration(info.handle_id));
  EXPECT_EQ(0UL, ipc_sink()->message_count());
}

TEST_F(ServiceWorkerDispatcherTest, FindOrCreateRegistration) {
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Should return a registration object newly created with adopting refcounts.
  scoped_ptr<WebServiceWorkerRegistrationImpl> registration(
      FindOrCreateRegistration(info, attrs));
  EXPECT_TRUE(registration);
  EXPECT_EQ(info.registration_id, registration->registration_id());
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Should return the existing registration object with adopting refcounts.
  WebServiceWorkerRegistrationImpl* existing_registration =
      FindOrCreateRegistration(info, attrs);
  EXPECT_EQ(registration, existing_registration);
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());
}

}  // namespace content

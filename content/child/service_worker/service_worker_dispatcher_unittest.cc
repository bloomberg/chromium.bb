// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    dispatcher_.reset(new ServiceWorkerDispatcher(sender_.get(), nullptr));
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
  scoped_refptr<WebServiceWorkerImpl> worker(
      dispatcher()->GetOrCreateServiceWorker(attrs.installing));
  EXPECT_TRUE(worker);
  EXPECT_TRUE(ContainsServiceWorker(attrs.installing.handle_id));
  EXPECT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());

  ipc_sink()->ClearMessages();

  // Should return the existing worker object.
  scoped_refptr<WebServiceWorkerImpl> existing_worker =
      dispatcher()->GetOrCreateServiceWorker(attrs.installing);
  EXPECT_EQ(worker, existing_worker);
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Should return the existing worker object with adopting refcount.
  existing_worker = dispatcher()->GetOrAdoptServiceWorker(attrs.installing);
  EXPECT_EQ(worker, existing_worker);
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());

  ipc_sink()->ClearMessages();

  // Should return another worker object newly created with adopting refcount.
  scoped_refptr<WebServiceWorkerImpl> another_worker(
      dispatcher()->GetOrAdoptServiceWorker(attrs.waiting));
  EXPECT_NE(worker.get(), another_worker.get());
  EXPECT_TRUE(ContainsServiceWorker(attrs.waiting.handle_id));
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Should return nullptr when a given object is invalid.
  scoped_refptr<WebServiceWorkerImpl> invalid_worker =
      dispatcher()->GetOrCreateServiceWorker(ServiceWorkerObjectInfo());
  EXPECT_FALSE(invalid_worker);
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  invalid_worker =
      dispatcher()->GetOrAdoptServiceWorker(ServiceWorkerObjectInfo());
  EXPECT_FALSE(invalid_worker);
  EXPECT_EQ(0UL, ipc_sink()->message_count());
}

TEST_F(ServiceWorkerDispatcherTest, GetOrCreateRegistration) {
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Should return a registration object newly created with incrementing
  // the refcounts.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration1(
      dispatcher()->GetOrCreateRegistration(info, attrs));
  EXPECT_TRUE(registration1);
  EXPECT_TRUE(ContainsRegistration(info.handle_id));
  EXPECT_EQ(info.registration_id, registration1->registration_id());
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());

  ipc_sink()->ClearMessages();

  // Should return the same registration object without incrementing the
  // refcounts.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration2(
      dispatcher()->GetOrCreateRegistration(info, attrs));
  EXPECT_TRUE(registration2);
  EXPECT_EQ(registration1, registration2);
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  ipc_sink()->ClearMessages();

  // The registration dtor decrements the refcounts.
  registration1 = nullptr;
  registration2 = nullptr;
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());
}

TEST_F(ServiceWorkerDispatcherTest, GetOrAdoptRegistration) {
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Should return a registration object newly created with adopting the
  // refcounts.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration1(
      dispatcher()->GetOrAdoptRegistration(info, attrs));
  EXPECT_TRUE(registration1);
  EXPECT_TRUE(ContainsRegistration(info.handle_id));
  EXPECT_EQ(info.registration_id, registration1->registration_id());
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  ipc_sink()->ClearMessages();

  // Should return the same registration object without incrementing the
  // refcounts.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration2(
      dispatcher()->GetOrAdoptRegistration(info, attrs));
  EXPECT_TRUE(registration2);
  EXPECT_EQ(registration1, registration2);
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());

  ipc_sink()->ClearMessages();

  // The registration dtor decrements the refcounts.
  registration1 = nullptr;
  registration2 = nullptr;
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());
}

}  // namespace content

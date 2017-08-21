// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_provider_context.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_provider_interfaces.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

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

  ThreadSafeSender* thread_safe_sender() { return sender_.get(); }
  IPC::TestSink* ipc_sink() { return &ipc_sink_; }
  ServiceWorkerDispatcher* dispatcher() { return dispatcher_.get(); }

 private:
  base::MessageLoop message_loop_;
  IPC::TestSink ipc_sink_;
  std::unique_ptr<ServiceWorkerDispatcher> dispatcher_;
  scoped_refptr<ServiceWorkerTestSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderContextTest);
};

TEST_F(ServiceWorkerProviderContextTest, CreateForController) {
  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  ServiceWorkerRegistrationObjectInfo registration_info;
  ServiceWorkerVersionAttributes version_attrs;
  CreateObjectInfoAndVersionAttributes(&registration_info, &version_attrs);
  std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration =
      ServiceWorkerRegistrationHandleReference::Adopt(registration_info,
                                                      thread_safe_sender());
  std::unique_ptr<ServiceWorkerHandleReference> installing =
      ServiceWorkerHandleReference::Adopt(version_attrs.installing,
                                          thread_safe_sender());
  std::unique_ptr<ServiceWorkerHandleReference> waiting =
      ServiceWorkerHandleReference::Adopt(version_attrs.waiting,
                                          thread_safe_sender());
  std::unique_ptr<ServiceWorkerHandleReference> active =
      ServiceWorkerHandleReference::Adopt(version_attrs.active,
                                          thread_safe_sender());

  // Set up ServiceWorkerProviderContext for ServiceWorkerGlobalScope.
  const int kProviderId = 10;
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, SERVICE_WORKER_PROVIDER_FOR_CONTROLLER, nullptr, nullptr,
      dispatcher());

  // The passed references should be adopted and owned by the provider context.
  provider_context->SetRegistration(std::move(registration),
                                    std::move(installing), std::move(waiting),
                                    std::move(active));
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Destruction of the provider context should release references to the
  // associated registration and its versions.
  provider_context = nullptr;
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

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerDispatcherHostTest : public testing::Test {
 protected:
  ServiceWorkerDispatcherHostTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() {
    context_wrapper_ = new ServiceWorkerContextWrapper;
    context_wrapper_->Init(base::FilePath(), NULL);
  }

  virtual void TearDown() {
    if (context_wrapper_) {
      context_wrapper_->Shutdown();
      context_wrapper_ = NULL;
    }
  }

  ServiceWorkerContextCore* context() { return context_wrapper_->context(); }

  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;
};

static const int kRenderProcessId = 1;

class TestingServiceWorkerDispatcherHost : public ServiceWorkerDispatcherHost {
 public:
  TestingServiceWorkerDispatcherHost(
      int process_id,
      ServiceWorkerContextWrapper* context_wrapper)
      : ServiceWorkerDispatcherHost(process_id),
        bad_messages_received_count_(0) {
    Init(context_wrapper);
  }

  virtual bool Send(IPC::Message* message) OVERRIDE {
    sent_messages_.push_back(message);
    return true;
  }

  virtual void BadMessageReceived() OVERRIDE {
    ++bad_messages_received_count_;
  }

  ScopedVector<IPC::Message> sent_messages_;
  int bad_messages_received_count_;

 protected:
  virtual ~TestingServiceWorkerDispatcherHost() {}
};

TEST_F(ServiceWorkerDispatcherHostTest, DisabledCausesError) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));

  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(kRenderProcessId,
                                             context_wrapper_.get());

  bool handled;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, GURL(), GURL()),
      &handled);
  EXPECT_TRUE(handled);

  // TODO(alecflett): Pump the message loop when this becomes async.
  ASSERT_EQ(1UL, dispatcher_host->sent_messages_.size());
  EXPECT_EQ(
      static_cast<uint32>(ServiceWorkerMsg_ServiceWorkerRegistrationError::ID),
      dispatcher_host->sent_messages_[0]->type());
}

TEST_F(ServiceWorkerDispatcherHostTest, Enabled) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableServiceWorker);

  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(kRenderProcessId,
                                             context_wrapper_.get());

  bool handled;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, GURL(), GURL()),
      &handled);
  EXPECT_TRUE(handled);
  base::RunLoop().RunUntilIdle();

  // TODO(alecflett): Pump the message loop when this becomes async.
  ASSERT_EQ(1UL, dispatcher_host->sent_messages_.size());
  EXPECT_EQ(static_cast<uint32>(ServiceWorkerMsg_ServiceWorkerRegistered::ID),
            dispatcher_host->sent_messages_[0]->type());
}

TEST_F(ServiceWorkerDispatcherHostTest, EarlyContextDeletion) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableServiceWorker);

  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(kRenderProcessId,
                                             context_wrapper_.get());

  context_wrapper_->Shutdown();
  context_wrapper_ = NULL;

  bool handled;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, GURL(), GURL()),
      &handled);
  EXPECT_TRUE(handled);

  // TODO(alecflett): Pump the message loop when this becomes async.
  ASSERT_EQ(1UL, dispatcher_host->sent_messages_.size());
  EXPECT_EQ(
      static_cast<uint32>(ServiceWorkerMsg_ServiceWorkerRegistrationError::ID),
      dispatcher_host->sent_messages_[0]->type());
}

TEST_F(ServiceWorkerDispatcherHostTest, ProviderCreatedAndDestroyed) {
  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(kRenderProcessId,
                                             context_wrapper_.get());

  const int kProviderId = 1001;  // Test with a value != kRenderProcessId.

  bool handled = false;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderCreated(kProviderId),
      &handled);
  EXPECT_TRUE(handled);
  EXPECT_TRUE(context()->GetProviderHost(kRenderProcessId, kProviderId));

  // Two with the same ID should be seen as a bad message.
  handled = false;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderCreated(kProviderId),
      &handled);
  EXPECT_TRUE(handled);
  EXPECT_EQ(1, dispatcher_host->bad_messages_received_count_);

  handled = false;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderDestroyed(kProviderId),
      &handled);
  EXPECT_TRUE(handled);
  EXPECT_FALSE(context()->GetProviderHost(kRenderProcessId, kProviderId));

  // Destroying an ID that does not exist warrants a bad message.
  handled = false;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderDestroyed(kProviderId),
      &handled);
  EXPECT_TRUE(handled);
  EXPECT_EQ(2, dispatcher_host->bad_messages_received_count_);

  // Deletion of the dispatcher_host should cause providers for that
  // process to get deleted as well.
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderCreated(kProviderId),
      &handled);
  EXPECT_TRUE(handled);
  EXPECT_TRUE(context()->GetProviderHost(kRenderProcessId, kProviderId));
  EXPECT_TRUE(dispatcher_host->HasOneRef());
  dispatcher_host = NULL;
  EXPECT_FALSE(context()->GetProviderHost(kRenderProcessId, kProviderId));
}

}  // namespace content

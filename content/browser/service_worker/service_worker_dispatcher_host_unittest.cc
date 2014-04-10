// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

static const int kRenderProcessId = 1;

class ServiceWorkerDispatcherHostTest : public testing::Test {
 protected:
  ServiceWorkerDispatcherHostTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() {
    context_wrapper_ = new ServiceWorkerContextWrapper;
    context_wrapper_->Init(base::FilePath(), NULL);
    helper_.reset(new EmbeddedWorkerTestHelper(context(), kRenderProcessId));
  }

  virtual void TearDown() {
    helper_.reset();
    if (context_wrapper_) {
      context_wrapper_->Shutdown();
      context_wrapper_ = NULL;
    }
  }

  ServiceWorkerContextCore* context() { return context_wrapper_->context(); }

  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
};


class TestingServiceWorkerDispatcherHost : public ServiceWorkerDispatcherHost {
 public:
  TestingServiceWorkerDispatcherHost(
      int process_id,
      ServiceWorkerContextWrapper* context_wrapper,
      EmbeddedWorkerTestHelper* helper)
      : ServiceWorkerDispatcherHost(process_id, NULL),
        bad_messages_received_count_(0),
        helper_(helper) {
    Init(context_wrapper);
  }

  virtual bool Send(IPC::Message* message) OVERRIDE {
    return helper_->Send(message);
  }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  virtual void BadMessageReceived() OVERRIDE {
    ++bad_messages_received_count_;
  }

  int bad_messages_received_count_;

 protected:
  EmbeddedWorkerTestHelper* helper_;
  virtual ~TestingServiceWorkerDispatcherHost() {}
};

TEST_F(ServiceWorkerDispatcherHostTest, DisabledCausesError) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));

  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(
          kRenderProcessId, context_wrapper_.get(), helper_.get());

  bool handled;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, -1, GURL(), GURL()),
      &handled);
  EXPECT_TRUE(handled);

  // TODO(alecflett): Pump the message loop when this becomes async.
  ASSERT_EQ(1UL, dispatcher_host->ipc_sink()->message_count());
  EXPECT_TRUE(dispatcher_host->ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_ServiceWorkerRegistrationError::ID));
}

// Disable this since now we cache command-line switch in
// ServiceWorkerUtils::IsFeatureEnabled() and this could be flaky depending
// on testing order. (crbug.com/352581)
// TODO(kinuko): Just remove DisabledCausesError test above and enable
// this test when we remove the --enable-service-worker flag.
TEST_F(ServiceWorkerDispatcherHostTest, DISABLED_Enabled) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableServiceWorker);

  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(
          kRenderProcessId, context_wrapper_.get(), helper_.get());

  bool handled;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, -1, GURL(), GURL()),
      &handled);
  EXPECT_TRUE(handled);
  base::RunLoop().RunUntilIdle();

  // TODO(alecflett): Pump the message loop when this becomes async.
  ASSERT_EQ(2UL, dispatcher_host->ipc_sink()->message_count());
  EXPECT_TRUE(dispatcher_host->ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StartWorker::ID));
  EXPECT_TRUE(dispatcher_host->ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_ServiceWorkerRegistered::ID));
}

TEST_F(ServiceWorkerDispatcherHostTest, EarlyContextDeletion) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableServiceWorker);

  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(
          kRenderProcessId, context_wrapper_.get(), helper_.get());

  context_wrapper_->Shutdown();
  context_wrapper_ = NULL;

  bool handled;
  dispatcher_host->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, -1, GURL(), GURL()),
      &handled);
  EXPECT_TRUE(handled);

  // TODO(alecflett): Pump the message loop when this becomes async.
  ASSERT_EQ(1UL, dispatcher_host->ipc_sink()->message_count());
  EXPECT_TRUE(dispatcher_host->ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_ServiceWorkerRegistrationError::ID));
}

TEST_F(ServiceWorkerDispatcherHostTest, ProviderCreatedAndDestroyed) {
  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host =
      new TestingServiceWorkerDispatcherHost(
          kRenderProcessId, context_wrapper_.get(), helper_.get());

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

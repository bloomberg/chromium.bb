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

class ServiceWorkerDispatcherHostTest : public testing::Test {
 protected:
  ServiceWorkerDispatcherHostTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() {
    helper_.reset(new EmbeddedWorkerTestHelper(kRenderProcessId));
    dispatcher_host_ = new TestingServiceWorkerDispatcherHost(
        kRenderProcessId, context_wrapper(), helper_.get());
  }

  virtual void TearDown() {
    helper_.reset();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() {
    return helper_->context_wrapper();
  }

  void SendRegister(int64 provider_id, GURL pattern, GURL worker_url) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_RegisterServiceWorker(
            -1, -1, provider_id, pattern, worker_url));
    base::RunLoop().RunUntilIdle();
  }

  void Register(int64 provider_id,
                GURL pattern,
                GURL worker_url,
                uint32 expected_message) {
    SendRegister(provider_id, pattern, worker_url);
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  void SendUnregister(int64 provider_id, GURL pattern) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_UnregisterServiceWorker(
            -1, -1, provider_id, pattern));
    base::RunLoop().RunUntilIdle();
  }

  void Unregister(int64 provider_id, GURL pattern, uint32 expected_message) {
    SendUnregister(provider_id, pattern);
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host_;
};

TEST_F(ServiceWorkerDispatcherHostTest, Register_SameOrigin) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      kRenderProcessId, kProviderId, context()->AsWeakPtr(), NULL));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  base::WeakPtr<ServiceWorkerProviderHost> provider_host = host->AsWeakPtr();
  context()->AddProviderHost(host.Pass());

  Register(kProviderId,
           GURL("https://www.example.com/*"),
           GURL("https://www.example.com/bar"),
           ServiceWorkerMsg_ServiceWorkerRegistered::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_CrossOrigin) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      kRenderProcessId, kProviderId, context()->AsWeakPtr(), NULL));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  base::WeakPtr<ServiceWorkerProviderHost> provider_host = host->AsWeakPtr();
  context()->AddProviderHost(host.Pass());

  // Script has a different host
  SendRegister(kProviderId,
               GURL("https://www.example.com/*"),
               GURL("https://foo.example.com/bar"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  // Scope has a different host
  SendRegister(kProviderId,
               GURL("https://foo.example.com/*"),
               GURL("https://www.example.com/bar"));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  // Script has a different port
  SendRegister(kProviderId,
               GURL("https://www.example.com/*"),
               GURL("https://www.example.com:8080/bar"));
  EXPECT_EQ(3, dispatcher_host_->bad_messages_received_count_);

  // Scope has a different transport
  SendRegister(kProviderId,
               GURL("wss://www.example.com/*"),
               GURL("https://www.example.com/bar"));
  EXPECT_EQ(4, dispatcher_host_->bad_messages_received_count_);

  // Script and scope have different hosts
  SendRegister(kProviderId,
               GURL("https://foo.example.com/*"),
               GURL("https://foo.example.com/bar"));
  EXPECT_EQ(5, dispatcher_host_->bad_messages_received_count_);

  // Script and scope URLs are invalid
  SendRegister(kProviderId,
               GURL(),
               GURL("h@ttps://@"));
  EXPECT_EQ(6, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, Unregister_SameOrigin) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      kRenderProcessId, kProviderId, context()->AsWeakPtr(), NULL));
  host->SetDocumentUrl(GURL("http://www.example.com/foo"));
  base::WeakPtr<ServiceWorkerProviderHost> provider_host = host->AsWeakPtr();
  context()->AddProviderHost(host.Pass());

  Unregister(kProviderId,
             GURL("http://www.example.com/*"),
             ServiceWorkerMsg_ServiceWorkerUnregistered::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, Unregister_CrossOrigin) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      kRenderProcessId, kProviderId, context()->AsWeakPtr(), NULL));
  host->SetDocumentUrl(GURL("http://www.example.com/foo"));
  base::WeakPtr<ServiceWorkerProviderHost> provider_host = host->AsWeakPtr();
  context()->AddProviderHost(host.Pass());

  SendUnregister(kProviderId, GURL("http://foo.example.com/*"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, EarlyContextDeletion) {
  helper_->ShutdownContext();

  // Let the shutdown reach the simulated IO thread.
  base::RunLoop().RunUntilIdle();

  Register(-1,
           GURL(),
           GURL(),
           ServiceWorkerMsg_ServiceWorkerRegistrationError::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, ProviderCreatedAndDestroyed) {
  const int kProviderId = 1001;  // Test with a value != kRenderProcessId.

  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderCreated(kProviderId));
  EXPECT_TRUE(context()->GetProviderHost(kRenderProcessId, kProviderId));

  // Two with the same ID should be seen as a bad message.
  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderCreated(kProviderId));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderDestroyed(kProviderId));
  EXPECT_FALSE(context()->GetProviderHost(kRenderProcessId, kProviderId));

  // Destroying an ID that does not exist warrants a bad message.
  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderDestroyed(kProviderId));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  // Deletion of the dispatcher_host should cause providers for that
  // process to get deleted as well.
  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderCreated(kProviderId));
  EXPECT_TRUE(context()->GetProviderHost(kRenderProcessId, kProviderId));
  EXPECT_TRUE(dispatcher_host_->HasOneRef());
  dispatcher_host_ = NULL;
  EXPECT_FALSE(context()->GetProviderHost(kRenderProcessId, kProviderId));
}

}  // namespace content

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/network_delay_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/mock_resource_context.h"
#include "content/browser/renderer_host/dummy_resource_handler.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/renderer_host/resource_queue.h"
#include "content/browser/site_instance.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"

#include "testing/gtest/include/gtest/gtest.h"

using content::DummyResourceHandler;

namespace {

const char kTestUrl[] = "http://www.google.com/";
const char kTestData[] = "Hello, World!";
const char kTestExtensionId1[] = "jfjjgilipffmpphcikcmjdaoomecgelc";
const char kTestExtensionId2[] = "pjohnlkdpdolplmenneanegndccmdlpc";
const char kTestExtensionNoNetworkDelay[] = "aocebcndggcnnmflapdklcmnfojmkmie";

ResourceDispatcherHostRequestInfo* CreateRequestInfo(int request_id) {
  return new ResourceDispatcherHostRequestInfo(
      new DummyResourceHandler(), content::PROCESS_TYPE_RENDERER, 0, 0, 0,
      request_id, false, -1, false, -1, ResourceType::MAIN_FRAME,
      content::PAGE_TRANSITION_LINK, 0, false, false, false,
      content::MockResourceContext::GetInstance());
}

// A simple test net::URLRequestJob. We don't care what it does, only whether
// it starts and finishes.
class SimpleTestJob : public net::URLRequestTestJob {
 public:
  explicit SimpleTestJob(net::URLRequest* request)
    : net::URLRequestTestJob(request, test_headers(), kTestData, true) {}
 private:
  ~SimpleTestJob() {}
};

// An ExtensionHost that doesn't require a Profile or TabContents, to use
// in notifications to the NetworkDelayListener.
class TestExtensionHost : public ExtensionHost {
 public:
  TestExtensionHost(const Extension* extension, content::ViewType host_type)
    : ExtensionHost(extension, host_type) {
  }
};

}  // namespace

class NetworkDelayListenerTest
    : public ExtensionServiceTestBase,
      public net::URLRequest::Interceptor {
 public:
  NetworkDelayListenerTest() {
    net::URLRequest::Deprecated::RegisterRequestInterceptor(this);
  }

  ~NetworkDelayListenerTest() {
    net::URLRequest::Deprecated::UnregisterRequestInterceptor(this);
  }

  virtual void SetUp() {
    ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();
    service_->Init();
    MessageLoop::current()->RunAllPending();

    listener_ = new NetworkDelayListener();

    ResourceQueue::DelegateSet delegates;
    delegates.insert(listener_.get());
    resource_queue_.Initialize(delegates);
  }

  virtual void TearDown() {
    resource_queue_.Shutdown();
    listener_ = NULL;
    MessageLoop::current()->RunAllPending();
  }

  // net::URLRequest::Interceptor
  virtual net::URLRequestJob* MaybeIntercept(net::URLRequest* request) {
    return new SimpleTestJob(request);
  }

 protected:
  TestURLRequest* StartTestRequest(net::URLRequest::Delegate* delegate,
                                   const std::string& url) {
    TestURLRequest* request = new TestURLRequest(GURL(url), delegate);
    scoped_ptr<ResourceDispatcherHostRequestInfo> rdh_info(
        CreateRequestInfo(0));
    resource_queue_.AddRequest(request, *rdh_info.get());
    return request;
  }

  void LoadTestExtension(const char* id) {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    FilePath extension_path = test_dir
        .AppendASCII("extensions")
        .AppendASCII("network_delay")
        .AppendASCII(id)
        .AppendASCII("1.0");

    extensions::UnpackedInstaller::Create(service_)->Load(extension_path);
    MessageLoop::current()->RunAllPending();
  }

  void LoadTestExtension1() {
    LoadTestExtension(kTestExtensionId1);
    ASSERT_FALSE(service_->extensions()->empty());
    extension1_ = service_->extensions()->at(0).get();
    ASSERT_FALSE(extension1_ == NULL);
  }

  void SendExtensionLoadedNotification(const Extension* extension) {
    scoped_ptr<TestExtensionHost> background_host(
        new TestExtensionHost(extension,
                              chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE));
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
        content::Source<Profile>(profile_.get()),
        content::Details<ExtensionHost>(background_host.get()));
    MessageLoop::current()->RunAllPending();
  }

  scoped_refptr<NetworkDelayListener> listener_;

  // Weak reference.
  const Extension* extension1_;

 private:
  ResourceQueue resource_queue_;
};

namespace {

// Tests that an extension delays network requests, and that loading its
// background page releases the block.
TEST_F(NetworkDelayListenerTest, DelayAndLoad) {
  LoadTestExtension1();

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kTestUrl));
  ASSERT_FALSE(request->is_pending());

  // We don't care about a loaded extension dialog.
  scoped_ptr<TestExtensionHost> dialog_host(
      new TestExtensionHost(extension1_, chrome::VIEW_TYPE_EXTENSION_DIALOG));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
      content::Source<Profile>(profile_.get()),
      content::Details<ExtensionHost>(dialog_host.get()));
  MessageLoop::current()->RunAllPending();

  SendExtensionLoadedNotification(extension1_);
  EXPECT_EQ(kTestData, delegate.data_received());
}

// Tests that an extension delays network requests, and that unloading it
// releases the block.
TEST_F(NetworkDelayListenerTest, DelayAndUnload) {
  LoadTestExtension1();

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kTestUrl));
  ASSERT_FALSE(request->is_pending());

  service_->UnloadExtension(extension1_->id(),
                            extension_misc::UNLOAD_REASON_DISABLE);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kTestData, delegate.data_received());
}

// Tests that two blocking extensions must both load for the block to be
// released.
TEST_F(NetworkDelayListenerTest, TwoBlockingExtensions) {
  LoadTestExtension1();
  LoadTestExtension(kTestExtensionId2);
  ASSERT_EQ(2u, service_->extensions()->size());
  const Extension* extension2 = service_->extensions()->at(1).get();
  ASSERT_FALSE(extension2 == NULL);

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kTestUrl));
  ASSERT_FALSE(request->is_pending());

  SendExtensionLoadedNotification(extension1_);
  ASSERT_FALSE(request->is_pending());

  SendExtensionLoadedNotification(extension2);
  EXPECT_EQ(kTestData, delegate.data_received());
}

// Tests that unloading a fully loaded extension (i.e., saying it's "ready"
// twice) doesn't crash.
TEST_F(NetworkDelayListenerTest, ExtensionReadyTwice) {
  LoadTestExtension1();

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kTestUrl));
  ASSERT_FALSE(request->is_pending());

  SendExtensionLoadedNotification(extension1_);
  EXPECT_EQ(kTestData, delegate.data_received());

  service_->UnloadExtension(extension1_->id(),
                            extension_misc::UNLOAD_REASON_DISABLE);
  MessageLoop::current()->RunAllPending();
}

// Tests that there's no delay if no loaded extension needs one.
TEST_F(NetworkDelayListenerTest, NoDelayNoWebRequest) {
  LoadTestExtension(kTestExtensionNoNetworkDelay);
  ASSERT_FALSE(service_->extensions()->empty());

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kTestUrl));

  // The request should be started immediately.
  ASSERT_TRUE(request->is_pending());

  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kTestData, delegate.data_received());
}

// Tests that there's no delay if no extensions are loaded.
TEST_F(NetworkDelayListenerTest, NoDelayNoExtensions) {
  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kTestUrl));

  // The request should be started immediately.
  ASSERT_TRUE(request->is_pending());

  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kTestData, delegate.data_received());
}

}  // namespace

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/renderer_host/global_request_id.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/browser/renderer_host/resource_handler.h"
#include "chrome/browser/renderer_host/resource_queue.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

class Profile;

namespace {

const char kMatchingUrl[] = "http://google.com/";
const char kNotMatchingUrl[] = "http://example.com/";
const char kTestData[] = "Hello, World!";

// Dummy ResourceHandler required for ResourceDispatcherHostRequestInfo.
class DummyResourceHandler : public ResourceHandler {
 public:
  DummyResourceHandler() {
  }

  virtual bool OnUploadProgress(int request_id, uint64 position, uint64 size) {
    NOTREACHED();
    return true;
  }


  virtual bool OnRequestRedirected(int request_id, const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) {
    NOTREACHED();
    return true;
  }

  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response) {
    NOTREACHED();
    return true;
  }

  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) {
    NOTREACHED();
    return true;
  }

  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) {
    NOTREACHED();
    return true;
  }

  virtual bool OnReadCompleted(int request_id, int* bytes_read) {
    NOTREACHED();
    return true;
  }

  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) {
    NOTREACHED();
    return true;
  }

  virtual void OnRequestClosed() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyResourceHandler);
};

ResourceDispatcherHostRequestInfo* CreateRequestInfo(int request_id) {
  return new ResourceDispatcherHostRequestInfo(
      new DummyResourceHandler(), ChildProcessInfo::RENDER_PROCESS, 0, 0,
      request_id, "null", "null", ResourceType::MAIN_FRAME,
      0, false, false, false, -1, -1);
}

// A simple test net::URLRequestJob. We don't care what it does, only that
// whether it starts and finishes.
class SimpleTestJob : public net::URLRequestTestJob {
 public:
  explicit SimpleTestJob(net::URLRequest* request)
    : net::URLRequestTestJob(request, test_headers(), kTestData, true) {}
 private:
  ~SimpleTestJob() {}
};

class UserScriptListenerTest
    : public ExtensionServiceTestBase,
      public net::URLRequest::Interceptor {
 public:
  UserScriptListenerTest() {
    net::URLRequest::RegisterRequestInterceptor(this);
  }

  ~UserScriptListenerTest() {
    net::URLRequest::UnregisterRequestInterceptor(this);
  }

  virtual void SetUp() {
    ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();
    service_->Init();
    MessageLoop::current()->RunAllPending();

    listener_ = new UserScriptListener(&resource_queue_);

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

  void LoadTestExtension() {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    FilePath extension_path = test_dir
        .AppendASCII("extensions")
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
        .AppendASCII("1.0.0.0");
    service_->LoadExtension(extension_path);
  }

  void UnloadTestExtension() {
    ASSERT_FALSE(service_->extensions()->empty());
    service_->UnloadExtension(service_->extensions()->at(0)->id(),
                              UnloadedExtensionInfo::DISABLE);
  }

  scoped_refptr<UserScriptListener> listener_;

 private:
  ResourceQueue resource_queue_;
};

TEST_F(UserScriptListenerTest, DelayAndUpdate) {
  LoadTestExtension();
  MessageLoop::current()->RunAllPending();

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kMatchingUrl));
  ASSERT_FALSE(request->is_pending());

  NotificationService::current()->Notify(
      NotificationType::USER_SCRIPTS_UPDATED,
      Source<Profile>(profile_.get()),
      NotificationService::NoDetails());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kTestData, delegate.data_received());
}

TEST_F(UserScriptListenerTest, DelayAndUnload) {
  LoadTestExtension();
  MessageLoop::current()->RunAllPending();

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kMatchingUrl));
  ASSERT_FALSE(request->is_pending());

  UnloadTestExtension();
  MessageLoop::current()->RunAllPending();

  // This is still not enough to start delayed requests. We have to notify the
  // listener that the user scripts have been updated.
  ASSERT_FALSE(request->is_pending());

  NotificationService::current()->Notify(
      NotificationType::USER_SCRIPTS_UPDATED,
      Source<Profile>(profile_.get()),
      NotificationService::NoDetails());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kTestData, delegate.data_received());
}

TEST_F(UserScriptListenerTest, NoDelayNoExtension) {
  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kMatchingUrl));

  // The request should be started immediately.
  ASSERT_TRUE(request->is_pending());

  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kTestData, delegate.data_received());
}

TEST_F(UserScriptListenerTest, NoDelayNotMatching) {
  LoadTestExtension();
  MessageLoop::current()->RunAllPending();

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate,
                                                      kNotMatchingUrl));

  // The request should be started immediately.
  ASSERT_TRUE(request->is_pending());

  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kTestData, delegate.data_received());
}

}  // namespace

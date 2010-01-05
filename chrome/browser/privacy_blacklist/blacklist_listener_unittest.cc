// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_listener.h"

#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_request_info.h"
#include "chrome/browser/privacy_blacklist/blacklist_test_util.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/browser/renderer_host/resource_handler.h"
#include "chrome/browser/renderer_host/resource_queue.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestUrl[] = "data:text/plain,Hello World!";

// Dummy ResourceHandler required for ResourceDispatcherHostRequestInfo.
class DummyResourceHandler : public ResourceHandler {
 public:
  DummyResourceHandler() {
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
                                   const URLRequestStatus& status,
                                   const std::string& security_info) {
    NOTREACHED();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyResourceHandler);
};

ResourceDispatcherHostRequestInfo* GetRequestInfo(int request_id) {
  return new ResourceDispatcherHostRequestInfo(
      new DummyResourceHandler(), ChildProcessInfo::RENDER_PROCESS, 0, 0,
      request_id, "null", "null", ResourceType::MAIN_FRAME,
      0, false, false, -1, -1);
}

class BlacklistListenerTest
    : public testing::Test,
      public NotificationObserver {
 public:
  BlacklistListenerTest()
      : path_provider_(&profile_),
        loop_(MessageLoop::TYPE_IO),
        mock_ui_thread_(ChromeThread::UI, MessageLoop::current()),
        mock_file_thread_(ChromeThread::FILE, MessageLoop::current()),
        mock_io_thread_(ChromeThread::IO, MessageLoop::current()) {
  }

  virtual void SetUp() {
    blacklist_manager_ = new BlacklistManager(&profile_, &path_provider_);
    blacklist_listener_ = new BlacklistListener(&resource_queue_);

    profile_.set_blacklist_manager(blacklist_manager_.get());

    ResourceQueue::DelegateSet delegates;
    delegates.insert(blacklist_listener_.get());
    resource_queue_.Initialize(delegates);
  }

  virtual void TearDown() {
    resource_queue_.Shutdown();
    blacklist_listener_ = NULL;
    blacklist_manager_ = NULL;
    loop_.RunAllPending();
  }

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    MessageLoop::current()->Quit();
  }

 protected:
  void WaitForBlacklistUpdate() {
    NotificationRegistrar registrar;
    registrar.Add(this,
                  NotificationType::BLACKLIST_MANAGER_BLACKLIST_READ_FINISHED,
                  Source<Profile>(&profile_));
    MessageLoop::current()->Run();
  }

  TestURLRequest* StartTestRequest(URLRequest::Delegate* delegate) {
    TestURLRequest* request = new TestURLRequest(GURL(kTestUrl), delegate);
    BlacklistRequestInfo* request_info =
        new BlacklistRequestInfo(request->url(), ResourceType::MAIN_FRAME,
                                 blacklist_manager_.get());
    request->SetUserData(&BlacklistRequestInfo::kURLRequestDataKey,
                         request_info);
    scoped_ptr<ResourceDispatcherHostRequestInfo> rdh_info(GetRequestInfo(0));
    resource_queue_.AddRequest(request, *rdh_info.get());
    return request;
  }

  BlacklistTestingProfile profile_;

  TestBlacklistPathProvider path_provider_;

  scoped_refptr<BlacklistManager> blacklist_manager_;

  scoped_refptr<BlacklistListener> blacklist_listener_;

 private:
  MessageLoop loop_;

  ChromeThread mock_ui_thread_;
  ChromeThread mock_file_thread_;
  ChromeThread mock_io_thread_;

  ResourceQueue resource_queue_;
};

TEST_F(BlacklistListenerTest, Delay) {
  blacklist_manager_->Initialize();

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate));

  // The request should get delayed, because the BlacklistManager is not yet
  // ready. When we run pending tasks, it should initialize itself, notify
  // the BlacklistListener, and start the request.
  ASSERT_FALSE(request->is_pending());

  MessageLoop::current()->RunAllPending();
  EXPECT_EQ("Hello World!", delegate.data_received());
}

TEST_F(BlacklistListenerTest, NoDelay) {
  blacklist_manager_->Initialize();

  // Make sure that the BlacklistManager is ready before we start the request.
  WaitForBlacklistUpdate();

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate));

  // The request should be started immediately.
  ASSERT_TRUE(request->is_pending());

  MessageLoop::current()->RunAllPending();
  EXPECT_EQ("Hello World!", delegate.data_received());
}

}  // namespace

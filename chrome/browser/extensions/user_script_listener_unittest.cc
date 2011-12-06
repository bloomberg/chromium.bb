// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/json/json_value_serializer.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/mock_resource_context.h"
#include "content/browser/renderer_host/dummy_resource_handler.h"
#include "content/browser/renderer_host/global_request_id.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/renderer_host/resource_queue.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"


class Profile;

using content::DummyResourceHandler;

namespace {

const char kMatchingUrl[] = "http://google.com/";
const char kNotMatchingUrl[] = "http://example.com/";
const char kTestData[] = "Hello, World!";

ResourceDispatcherHostRequestInfo* CreateRequestInfo(int request_id) {
  return new ResourceDispatcherHostRequestInfo(
      new DummyResourceHandler(), content::PROCESS_TYPE_RENDERER, 0, 0, 0,
      request_id, false, -1, false, -1, ResourceType::MAIN_FRAME,
      content::PAGE_TRANSITION_LINK, 0, false, false, false,
      WebKit::WebReferrerPolicyDefault,
      content::MockResourceContext::GetInstance());
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

// Yoinked from extension_manifest_unittest.cc.
DictionaryValue* LoadManifestFile(const FilePath path, std::string* error) {
  EXPECT_TRUE(file_util::PathExists(path));
  JSONFileValueSerializer serializer(path);
  return static_cast<DictionaryValue*>(serializer.Deserialize(NULL, error));
}

scoped_refptr<Extension> LoadExtension(const std::string& filename,
                                       std::string* error) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.
      AppendASCII("extensions").
      AppendASCII("manifest_tests").
      AppendASCII(filename.c_str());
  scoped_ptr<DictionaryValue> value(LoadManifestFile(path, error));
  if (!value.get())
    return NULL;
  return Extension::Create(path.DirName(), Extension::LOAD, *value,
                           Extension::NO_FLAGS, error);
}

}  // namespace

class UserScriptListenerTest
    : public ExtensionServiceTestBase,
      public net::URLRequest::Interceptor {
 public:
  UserScriptListenerTest() {
    net::URLRequest::Deprecated::RegisterRequestInterceptor(this);
  }

  ~UserScriptListenerTest() {
    net::URLRequest::Deprecated::UnregisterRequestInterceptor(this);
  }

  virtual void SetUp() {
    ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();
    service_->Init();
    MessageLoop::current()->RunAllPending();

    listener_ = new UserScriptListener();

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
    extensions::UnpackedInstaller::Create(service_)->Load(extension_path);
  }

  void UnloadTestExtension() {
    ASSERT_FALSE(service_->extensions()->is_empty());
    service_->UnloadExtension((*service_->extensions()->begin())->id(),
                              extension_misc::UNLOAD_REASON_DISABLE);
  }

  scoped_refptr<UserScriptListener> listener_;

 private:
  ResourceQueue resource_queue_;
};

namespace {

TEST_F(UserScriptListenerTest, DelayAndUpdate) {
  LoadTestExtension();
  MessageLoop::current()->RunAllPending();

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kMatchingUrl));
  ASSERT_FALSE(request->is_pending());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());
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

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());
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

TEST_F(UserScriptListenerTest, MultiProfile) {
  LoadTestExtension();
  MessageLoop::current()->RunAllPending();

  // Fire up a second profile and have it load and extension with a content
  // script.
  TestingProfile profile2;
  std::string error;
  scoped_refptr<Extension> extension = LoadExtension(
      "content_script_yahoo.json", &error);
  ASSERT_TRUE(extension.get());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(&profile2),
      content::Details<Extension>(extension.get()));

  TestDelegate delegate;
  scoped_ptr<TestURLRequest> request(StartTestRequest(&delegate, kMatchingUrl));
  ASSERT_FALSE(request->is_pending());

  // When the first profile's user scripts are ready, the request should still
  // be blocked waiting for profile2.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());
  MessageLoop::current()->RunAllPending();
  ASSERT_FALSE(request->is_pending());
  EXPECT_TRUE(delegate.data_received().empty());

  // After profile2 is ready, the request should proceed.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(&profile2),
      content::NotificationService::NoDetails());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kTestData, delegate.data_received());
}

}  // namespace

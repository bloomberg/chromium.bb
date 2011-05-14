// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "base/file_util.h"
#include "base/path_service.h"

#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/extensions/extension_webrequest_api_constants.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_webrequest_api_constants;

namespace {
static void EventHandledOnIOThread(
    ProfileId profile_id,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64 request_id,
    ExtensionWebRequestEventRouter::EventResponse* response) {
  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      profile_id, extension_id, event_name, sub_event_name, request_id,
      response);
}
}  // namespace

// A mock event router that responds to events with a pre-arranged queue of
// Tasks.
class TestEventRouter : public ExtensionEventRouterForwarder {
public:
  // Adds a Task to the queue. We will fire these in order as events are
  // dispatched.
  void PushTask(Task* task) {
    task_queue_.push(task);
  }

  size_t GetNumTasks() { return task_queue_.size(); }

private:
  // ExtensionEventRouterForwarder:
  virtual void HandleEvent(const std::string& extension_id,
                           const std::string& event_name,
                           const std::string& event_args,
                           ProfileId profile_id,
                           bool use_profile_to_restrict_events,
                           const GURL& event_url) {
    ASSERT_FALSE(task_queue_.empty());
    MessageLoop::current()->PostTask(FROM_HERE, task_queue_.front());
    task_queue_.pop();
  }

  std::queue<Task*> task_queue_;
};

class ExtensionWebRequestTest : public testing::Test {
protected:
  virtual void SetUp() {
    event_router_ = new TestEventRouter();
    enable_referrers_.Init(
        prefs::kEnableReferrers, profile_.GetTestingPrefService(), NULL);
    network_delegate_.reset(new ChromeNetworkDelegate(
        event_router_.get(), profile_.GetRuntimeId(),
        &enable_referrers_));
    context_ = new TestURLRequestContext();
    context_->set_network_delegate(network_delegate_.get());
  }

  MessageLoopForIO io_loop_;
  TestingProfile profile_;
  TestDelegate delegate_;
  BooleanPrefMember enable_referrers_;
  scoped_refptr<TestEventRouter> event_router_;
  scoped_ptr<ChromeNetworkDelegate> network_delegate_;
  scoped_refptr<TestURLRequestContext> context_;
};

// Tests that we handle disagreements among extensions about responses to
// blocking events by choosing the response from the most-recently-installed
// extension.
TEST_F(ExtensionWebRequestTest, BlockingEventPrecedence) {
  std::string extension1_id("1");
  std::string extension2_id("2");
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string kEventName(keys::kOnBeforeRequest);
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      profile_.GetRuntimeId(), extension1_id, kEventName,
      kEventName + "/1", filter,
      ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING);
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      profile_.GetRuntimeId(), extension2_id, kEventName,
      kEventName + "/2", filter,
      ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING);

  net::URLRequest request(GURL("about:blank"), &delegate_);
  request.set_context(context_);

  {
    // onBeforeRequest will be dispatched twice initially. The second response -
    // the redirect - should win, since it has a later |install_time|. The
    // redirect will dispatch another pair of onBeforeRequest. There, the first
    // response should win (later |install_time|).
    GURL redirect_url("about:redirected");
    ExtensionWebRequestEventRouter::EventResponse* response = NULL;

    // Extension1 response. Arrives first, but ignored due to install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    response->cancel = true;
    event_router_->PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            profile_.GetRuntimeId(), extension1_id,
            kEventName, kEventName + "/1", request.identifier(), response));

    // Extension2 response. Arrives second, and chosen because of install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    response->new_url = redirect_url;
    event_router_->PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            profile_.GetRuntimeId(), extension2_id,
            kEventName, kEventName + "/2", request.identifier(), response));

    // Extension2 response to the redirected URL. Arrives first, and chosen.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    event_router_->PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            profile_.GetRuntimeId(), extension2_id,
            kEventName, kEventName + "/2", request.identifier(), response));

    // Extension1 response to the redirected URL. Arrives second, and ignored.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    response->cancel = true;
    event_router_->PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            profile_.GetRuntimeId(), extension1_id,
            kEventName, kEventName + "/1", request.identifier(), response));

    request.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(!request.is_pending());
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
    EXPECT_EQ(0, request.status().os_error());
    EXPECT_EQ(redirect_url, request.url());
    EXPECT_EQ(2U, request.url_chain().size());
    EXPECT_EQ(0U, event_router_->GetNumTasks());
  }
}

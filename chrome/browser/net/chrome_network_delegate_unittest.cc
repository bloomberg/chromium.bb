// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/api/prefs/pref_member.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeNetworkDelegateTest : public testing::Test {
 protected:
  ChromeNetworkDelegateTest()
      : forwarder_(new extensions::EventRouterForwarder()) {
  }

  virtual void SetUp() OVERRIDE {
    never_throttle_requests_original_value_ =
        ChromeNetworkDelegate::g_never_throttle_requests_;
    ChromeNetworkDelegate::g_never_throttle_requests_ = false;
  }

  virtual void TearDown() OVERRIDE {
    ChromeNetworkDelegate::g_never_throttle_requests_ =
        never_throttle_requests_original_value_;
  }

  scoped_ptr<ChromeNetworkDelegate> CreateNetworkDelegate() {
    return scoped_ptr<ChromeNetworkDelegate>(new ChromeNetworkDelegate(
        forwarder_.get(), NULL, NULL, NULL, NULL, NULL, &pref_member_, NULL,
        NULL));
  }

  // Implementation moved here for access to private bits.
  void NeverThrottleLogicImpl() {
    scoped_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());

    TestURLRequestContext context;
    TestURLRequest extension_request(
        GURL("http://example.com/"), NULL, &context);
    extension_request.set_first_party_for_cookies(
        GURL("chrome-extension://abcdef/bingo.html"));
    TestURLRequest web_page_request(
        GURL("http://example.com/"), NULL, &context);
    web_page_request.set_first_party_for_cookies(
        GURL("http://example.com/helloworld.html"));

    ASSERT_TRUE(delegate->OnCanThrottleRequest(extension_request));
    ASSERT_FALSE(delegate->OnCanThrottleRequest(web_page_request));

    delegate->NeverThrottleRequests();
    ASSERT_TRUE(ChromeNetworkDelegate::g_never_throttle_requests_);
    ASSERT_FALSE(delegate->OnCanThrottleRequest(extension_request));
    ASSERT_FALSE(delegate->OnCanThrottleRequest(web_page_request));

    // Verify that the flag applies to later instances of the
    // ChromeNetworkDelegate.
    //
    // We test the side effects of the flag rather than just the flag
    // itself (which we did above) to help ensure that a changed
    // implementation would show the same behavior, i.e. all instances
    // of ChromeNetworkDelegate after the flag is set obey the flag.
    scoped_ptr<ChromeNetworkDelegate> second_delegate(CreateNetworkDelegate());
    ASSERT_FALSE(delegate->OnCanThrottleRequest(extension_request));
    ASSERT_FALSE(delegate->OnCanThrottleRequest(web_page_request));
  }

 private:
  bool never_throttle_requests_original_value_;
  MessageLoopForIO message_loop_;

  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
  BooleanPrefMember pref_member_;
};

TEST_F(ChromeNetworkDelegateTest, NeverThrottleLogic) {
  NeverThrottleLogicImpl();
}

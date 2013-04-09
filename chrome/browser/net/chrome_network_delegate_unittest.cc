// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/completion_callback.h"
#include "net/url_request/url_request.h"
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
    return make_scoped_ptr(
        new ChromeNetworkDelegate(forwarder_.get(), &pref_member_));
  }

  // Implementation moved here for access to private bits.
  void NeverThrottleLogicImpl() {
    scoped_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());

    net::TestURLRequestContext context;
    net::TestURLRequest extension_request(
        GURL("http://example.com/"), NULL, &context, NULL);
    extension_request.set_first_party_for_cookies(
        GURL("chrome-extension://abcdef/bingo.html"));
    net::TestURLRequest web_page_request(
        GURL("http://example.com/"), NULL, &context, NULL);
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

class ChromeNetworkDelegateSafeSearchTest : public testing::Test {
 public:
  ChromeNetworkDelegateSafeSearchTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO, &message_loop_),
        forwarder_(new extensions::EventRouterForwarder()) {
  }

  virtual void SetUp() OVERRIDE {
    ChromeNetworkDelegate::InitializePrefsOnUIThread(
        &enable_referrers_, NULL, &force_google_safe_search_,
        profile_.GetTestingPrefService());
  }

 protected:
  scoped_ptr<net::NetworkDelegate> CreateNetworkDelegate() {
    scoped_ptr<ChromeNetworkDelegate> network_delegate(
        new ChromeNetworkDelegate(forwarder_.get(), &enable_referrers_));
    network_delegate->set_force_google_safe_search(&force_google_safe_search_);
    return network_delegate.PassAs<net::NetworkDelegate>();
  }

  void SetSafeSearch(bool value) {
    force_google_safe_search_.SetValue(value);
  }

  void SetDelegate(net::NetworkDelegate* delegate) {
    network_delegate_ = delegate;
    context_.set_network_delegate(network_delegate_);
  }

  // Does a request using the |url_string| URL and verifies that the expected
  // string is equal to the query part (between ? and #) of the final url of
  // that request.
  void CheckAddedParameters(const std::string& url_string,
                            const std::string& expected_query_parameters) {
    // Show the URL in the trace so we know where we failed.
    SCOPED_TRACE(url_string);

    net::TestURLRequest request(
        GURL(url_string), &delegate_, &context_, network_delegate_);

    request.Start();
    MessageLoop::current()->RunUntilIdle();

    EXPECT_EQ(expected_query_parameters, request.url().query());
  }

 private:
  MessageLoopForIO message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
  TestingProfile profile_;
  BooleanPrefMember enable_referrers_;
  BooleanPrefMember force_google_safe_search_;
  scoped_ptr<net::URLRequest> request_;
  net::TestURLRequestContext context_;
  net::NetworkDelegate* network_delegate_;
  net::TestDelegate delegate_;
};

TEST_F(ChromeNetworkDelegateSafeSearchTest, SafeSearchOn) {
  // Tests with SafeSearch on, request parameters should be rewritten.
  const std::string kSafeParameter = chrome::kSafeSearchSafeParameter;
  const std::string kSsuiParameter = chrome::kSafeSearchSsuiParameter;
  const std::string kBothParameters = kSafeParameter + "&" + kSsuiParameter;
  SetSafeSearch(true);
  scoped_ptr<net::NetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  // Test the home page.
  CheckAddedParameters("http://google.com/", kBothParameters);

  // Test the search home page.
  CheckAddedParameters("http://google.com/webhp",
                       kBothParameters);

  // Test different valid search pages with parameters.
  CheckAddedParameters("http://google.com/search?q=google",
                       "q=google&" + kBothParameters);

  CheckAddedParameters("http://google.com/?q=google",
                       "q=google&" + kBothParameters);

  CheckAddedParameters("http://google.com/webhp?q=google",
                       "q=google&" + kBothParameters);

  // Test the valid pages with safe set to off.
  CheckAddedParameters("http://google.com/search?q=google&safe=off",
                       "q=google&" + kBothParameters);

  CheckAddedParameters("http://google.com/?q=google&safe=off",
                       "q=google&" + kBothParameters);

  CheckAddedParameters("http://google.com/webhp?q=google&safe=off",
                       "q=google&" + kBothParameters);

  CheckAddedParameters("http://google.com/webhp?q=google&%73afe=off",
                       "q=google&%73afe=off&" + kBothParameters);

  // Test the home page, different TLDs.
  CheckAddedParameters("http://google.de/", kBothParameters);
  CheckAddedParameters("http://google.ro/", kBothParameters);
  CheckAddedParameters("http://google.nl/", kBothParameters);

  // Test the search home page, different TLD.
  CheckAddedParameters("http://google.de/webhp", kBothParameters);

  // Test the search page with parameters, different TLD.
  CheckAddedParameters("http://google.de/search?q=google",
                       "q=google&" + kBothParameters);

  // Test the home page with parameters, different TLD.
  CheckAddedParameters("http://google.de/?q=google",
                       "q=google&" + kBothParameters);

  // Test the search page with the parameters set.
  CheckAddedParameters("http://google.de/?q=google&" + kBothParameters,
                       "q=google&" + kBothParameters);

  // Test some possibly tricky combinations.
  CheckAddedParameters("http://google.com/?q=goog&" + kSafeParameter +
                       "&ssui=one",
                       "q=goog&" + kBothParameters);

  CheckAddedParameters("http://google.de/?q=goog&unsafe=active&" +
                       kSsuiParameter,
                       "q=goog&unsafe=active&" + kBothParameters);

  CheckAddedParameters("http://google.de/?q=goog&safe=off&ssui=off",
                       "q=goog&" + kBothParameters);

  // Test various combinations where we should not add anything.
  CheckAddedParameters("http://google.com/?q=goog&" + kSsuiParameter + "&" +
                       kSafeParameter,
                       "q=goog&" + kBothParameters);

  CheckAddedParameters("http://google.com/?" + kSsuiParameter + "&q=goog&" +
                       kSafeParameter,
                       "q=goog&" + kBothParameters);

  CheckAddedParameters("http://google.com/?" + kSsuiParameter + "&" +
                       kSafeParameter + "&q=goog",
                       "q=goog&" + kBothParameters);

  // Test that another website is not affected, without parameters.
  CheckAddedParameters("http://google.com/finance", "");

  // Test that another website is not affected, with parameters.
  CheckAddedParameters("http://google.com/finance?q=goog", "q=goog");

  // Test that another website is not affected with redirects, with parameters.
  CheckAddedParameters("http://finance.google.com/?q=goog", "q=goog");

  // Test with percent-encoded data (%26 is &)
  CheckAddedParameters("http://google.com/?q=%26%26%26&" + kSsuiParameter +
                       "&" + kSafeParameter + "&param=%26%26%26",
                       "q=%26%26%26&param=%26%26%26&" + kBothParameters);
}

TEST_F(ChromeNetworkDelegateSafeSearchTest, SafeSearchOff) {
  // Tests with SafeSearch settings off, delegate should not alter requests.
  SetSafeSearch(false);
  scoped_ptr<net::NetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  // Test the home page.
  CheckAddedParameters("http://google.com/", "");

  // Test the search home page.
  CheckAddedParameters("http://google.com/webhp", "");

  // Test the home page with parameters.
  CheckAddedParameters("http://google.com/search?q=google",
                       "q=google");

  // Test the search page with parameters.
  CheckAddedParameters("http://google.com/?q=google",
                       "q=google");

  // Test the search webhp page with parameters.
  CheckAddedParameters("http://google.com/webhp?q=google",
                       "q=google");

  // Test the home page with parameters and safe set to off.
  CheckAddedParameters("http://google.com/search?q=google&safe=off",
                       "q=google&safe=off");

  // Test the home page with parameters and safe set to active.
  CheckAddedParameters("http://google.com/search?q=google&safe=active",
                       "q=google&safe=active");
}

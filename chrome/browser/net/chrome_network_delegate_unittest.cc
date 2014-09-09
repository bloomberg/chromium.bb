// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/completion_callback.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

#if defined(ENABLE_EXTENSIONS)
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
    scoped_ptr<net::URLRequest> extension_request(context.CreateRequest(
        GURL("http://example.com/"), net::DEFAULT_PRIORITY, NULL, NULL));
    extension_request->set_first_party_for_cookies(
        GURL("chrome-extension://abcdef/bingo.html"));
    scoped_ptr<net::URLRequest> web_page_request(context.CreateRequest(
        GURL("http://example.com/"), net::DEFAULT_PRIORITY, NULL, NULL));
    web_page_request->set_first_party_for_cookies(
        GURL("http://example.com/helloworld.html"));

    ASSERT_TRUE(delegate->OnCanThrottleRequest(*extension_request));
    ASSERT_FALSE(delegate->OnCanThrottleRequest(*web_page_request));

    delegate->NeverThrottleRequests();
    ASSERT_TRUE(ChromeNetworkDelegate::g_never_throttle_requests_);
    ASSERT_FALSE(delegate->OnCanThrottleRequest(*extension_request));
    ASSERT_FALSE(delegate->OnCanThrottleRequest(*web_page_request));

    // Verify that the flag applies to later instances of the
    // ChromeNetworkDelegate.
    //
    // We test the side effects of the flag rather than just the flag
    // itself (which we did above) to help ensure that a changed
    // implementation would show the same behavior, i.e. all instances
    // of ChromeNetworkDelegate after the flag is set obey the flag.
    scoped_ptr<ChromeNetworkDelegate> second_delegate(CreateNetworkDelegate());
    ASSERT_FALSE(delegate->OnCanThrottleRequest(*extension_request));
    ASSERT_FALSE(delegate->OnCanThrottleRequest(*web_page_request));
  }

 private:
  bool never_throttle_requests_original_value_;
  base::MessageLoopForIO message_loop_;

  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
  BooleanPrefMember pref_member_;
};

TEST_F(ChromeNetworkDelegateTest, NeverThrottleLogic) {
  NeverThrottleLogicImpl();
}
#endif  // defined(ENABLE_EXTENSIONS)

class ChromeNetworkDelegateSafeSearchTest : public testing::Test {
 public:
  ChromeNetworkDelegateSafeSearchTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
#if defined(ENABLE_EXTENSIONS)
    forwarder_ = new extensions::EventRouterForwarder();
#endif
  }

  virtual void SetUp() OVERRIDE {
    ChromeNetworkDelegate::InitializePrefsOnUIThread(
        &enable_referrers_, NULL, &force_google_safe_search_,
        profile_.GetTestingPrefService());
  }

 protected:
  scoped_ptr<net::NetworkDelegate> CreateNetworkDelegate() {
    scoped_ptr<ChromeNetworkDelegate> network_delegate(
        new ChromeNetworkDelegate(forwarder(), &enable_referrers_));
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

    scoped_ptr<net::URLRequest> request(context_.CreateRequest(
        GURL(url_string), net::DEFAULT_PRIORITY, &delegate_, NULL));

    request->Start();
    base::MessageLoop::current()->RunUntilIdle();

    EXPECT_EQ(expected_query_parameters, request->url().query());
  }

 private:
  extensions::EventRouterForwarder* forwarder() {
#if defined(ENABLE_EXTENSIONS)
    return forwarder_.get();
#else
    return NULL;
#endif
  }

  content::TestBrowserThreadBundle thread_bundle_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
#endif
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
  CheckAddedParameters("http://google.com/finance", std::string());

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
  CheckAddedParameters("http://google.com/", std::string());

  // Test the search home page.
  CheckAddedParameters("http://google.com/webhp", std::string());

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

// Privacy Mode disables Channel Id if cookies are blocked (cr223191)
class ChromeNetworkDelegatePrivacyModeTest : public testing::Test {
 public:
  ChromeNetworkDelegatePrivacyModeTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
#if defined(ENABLE_EXTENSIONS)
        forwarder_(new extensions::EventRouterForwarder()),
#endif
        cookie_settings_(CookieSettings::Factory::GetForProfile(&profile_)
                             .get()),
        kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kBlockedFirstPartySite("http://no.thirdparties.com") {}

  virtual void SetUp() OVERRIDE {
    ChromeNetworkDelegate::InitializePrefsOnUIThread(
        &enable_referrers_, NULL, NULL,
        profile_.GetTestingPrefService());
  }

 protected:
  scoped_ptr<ChromeNetworkDelegate> CreateNetworkDelegate() {
    scoped_ptr<ChromeNetworkDelegate> network_delegate(
        new ChromeNetworkDelegate(forwarder(), &enable_referrers_));
    network_delegate->set_cookie_settings(cookie_settings_);
    return network_delegate.Pass();
  }

  void SetDelegate(net::NetworkDelegate* delegate) {
    network_delegate_ = delegate;
    context_.set_network_delegate(network_delegate_);
  }

 protected:
  extensions::EventRouterForwarder* forwarder() {
#if defined(ENABLE_EXTENSIONS)
    return forwarder_.get();
#else
    return NULL;
#endif
  }

  content::TestBrowserThreadBundle thread_bundle_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
#endif
  TestingProfile profile_;
  CookieSettings* cookie_settings_;
  BooleanPrefMember enable_referrers_;
  scoped_ptr<net::URLRequest> request_;
  net::TestURLRequestContext context_;
  net::NetworkDelegate* network_delegate_;

  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kEmptyFirstPartySite;
  const GURL kFirstPartySite;
  const GURL kBlockedFirstPartySite;
};

TEST_F(ChromeNetworkDelegatePrivacyModeTest, DisablePrivacyIfCookiesAllowed) {
  scoped_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kEmptyFirstPartySite));
}


TEST_F(ChromeNetworkDelegatePrivacyModeTest, EnablePrivacyIfCookiesBlocked) {
  scoped_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kBlockedSite,
                                                       kEmptyFirstPartySite));

  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(network_delegate_->CanEnablePrivacyMode(kBlockedSite,
                                                      kEmptyFirstPartySite));
}

TEST_F(ChromeNetworkDelegatePrivacyModeTest, EnablePrivacyIfThirdPartyBlocked) {
  scoped_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kFirstPartySite));

  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                      kFirstPartySite));
  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, false);
  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kFirstPartySite));
}

TEST_F(ChromeNetworkDelegatePrivacyModeTest,
       DisablePrivacyIfOnlyFirstPartyBlocked) {
  scoped_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kBlockedFirstPartySite));

  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedFirstPartySite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_BLOCK);
  // Privacy mode is disabled as kAllowedSite is still getting cookies
  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kBlockedFirstPartySite));
}


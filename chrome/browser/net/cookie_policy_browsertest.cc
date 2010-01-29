// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/net/chrome_cookie_policy.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

class CookiePolicyBrowserTest : public InProcessBrowserTest {
 public:
  CookiePolicyBrowserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CookiePolicyBrowserTest);
};

// Visits a page that sets a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, AllowFirstPartyCookies) {
  HTTPTestServer* server = StartHTTPServer();
  ASSERT_TRUE(server != NULL);

  PrefService* prefs = browser()->profile()->GetPrefs();
  browser()->profile()->GetCookiePolicy()->set_type(
      net::CookiePolicy::BLOCK_THIRD_PARTY_COOKIES);
  net::CookiePolicy::Type policy_type = net::CookiePolicy::FromInt(
      prefs->GetInteger(prefs::kCookieBehavior));
  ASSERT_EQ(net::CookiePolicy::BLOCK_THIRD_PARTY_COOKIES, policy_type);

  net::CookieStore* cookie_store =
      browser()->profile()->GetRequestContext()->GetCookieStore();

  GURL url = server->TestServerPage("set-cookie?cookie1");

  std::string cookie = cookie_store->GetCookies(url);
  ASSERT_EQ("", cookie);

  ui_test_utils::NavigateToURL(browser(), url);

  cookie = cookie_store->GetCookies(url);
  EXPECT_EQ("cookie1", cookie);
}


// Visits a page that is a redirect across domain boundary to a page that sets
// a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       AllowFirstPartyCookiesRedirect) {
  HTTPTestServer* server = StartHTTPServer();
  ASSERT_TRUE(server != NULL);

  PrefService* prefs = browser()->profile()->GetPrefs();
  browser()->profile()->GetCookiePolicy()->set_type(
      net::CookiePolicy::BLOCK_THIRD_PARTY_COOKIES);
  net::CookiePolicy::Type policy_type = net::CookiePolicy::FromInt(
      prefs->GetInteger(prefs::kCookieBehavior));
  ASSERT_EQ(net::CookiePolicy::BLOCK_THIRD_PARTY_COOKIES, policy_type);

  net::CookieStore* cookie_store =
      browser()->profile()->GetRequestContext()->GetCookieStore();

  GURL url = server->TestServerPage("server-redirect?");

  GURL redirected_url = server->TestServerPage("set-cookie?cookie2");
  // Change the host name from localhost to www.example.com so it triggers
  // third-party cookie blocking if the first party for cookies URL is not
  // changed when we follow a redirect.
  ASSERT_EQ("localhost", redirected_url.host());
  GURL::Replacements replacements;
  std::string new_host("www.example.com");
  replacements.SetHostStr(new_host);
  redirected_url = redirected_url.ReplaceComponents(replacements);

  std::string cookie = cookie_store->GetCookies(redirected_url);
  ASSERT_EQ("", cookie);

  host_resolver()->AddRule("www.example.com", "127.0.0.1");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(url.spec() + redirected_url.spec()));

  cookie = cookie_store->GetCookies(redirected_url);
  EXPECT_EQ("cookie2", cookie);
}

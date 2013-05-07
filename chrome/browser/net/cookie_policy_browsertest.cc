// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

using content::BrowserThread;

namespace {

class CookiePolicyBrowserTest : public InProcessBrowserTest {
 protected:
  CookiePolicyBrowserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CookiePolicyBrowserTest);
};

// Visits a page that sets a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, AllowFirstPartyCookies) {
  ASSERT_TRUE(test_server()->Start());

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies,
                                               true);

  GURL url(test_server()->GetURL("set-cookie?cookie1"));

  std::string cookie = content::GetCookies(browser()->profile(), url);
  ASSERT_EQ("", cookie);

  ui_test_utils::NavigateToURL(browser(), url);

  cookie = content::GetCookies(browser()->profile(), url);
  EXPECT_EQ("cookie1", cookie);
}

// Visits a page that is a redirect across domain boundary to a page that sets
// a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       AllowFirstPartyCookiesRedirect) {
  ASSERT_TRUE(test_server()->Start());

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies,
                                               true);

  GURL url(test_server()->GetURL("server-redirect?"));
  GURL redirected_url(test_server()->GetURL("set-cookie?cookie2"));

  // Change the host name from 127.0.0.1 to www.example.com so it triggers
  // third-party cookie blocking if the first party for cookies URL is not
  // changed when we follow a redirect.
  ASSERT_EQ("127.0.0.1", redirected_url.host());
  GURL::Replacements replacements;
  std::string new_host("www.example.com");
  replacements.SetHostStr(new_host);
  redirected_url = redirected_url.ReplaceComponents(replacements);

  std::string cookie =
      content::GetCookies(browser()->profile(), redirected_url);
  ASSERT_EQ("", cookie);

  host_resolver()->AddRule("www.example.com", "127.0.0.1");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(url.spec() + redirected_url.spec()));

  cookie = content::GetCookies(browser()->profile(), redirected_url);
  EXPECT_EQ("cookie2", cookie);
}

}  // namespace

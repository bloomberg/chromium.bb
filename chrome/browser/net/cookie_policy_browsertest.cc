// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/base/cookie_store.h"
#include "net/base/mock_host_resolver.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {

void GetCookiesCallback(std::string* cookies_out,
                        base::WaitableEvent* event,
                        const std::string& cookies) {
  *cookies_out = cookies;
  event->Signal();
}

void GetCookiesOnIOThread(const GURL& url,
                          net::URLRequestContextGetter* context_getter,
                          base::WaitableEvent* event,
                          std::string* cookies) {
  net::CookieStore* cookie_store =
      context_getter->GetURLRequestContext()->cookie_store();
  cookie_store->GetCookiesWithOptionsAsync(
      url, net::CookieOptions(),
      base::Bind(&GetCookiesCallback,
                 base::Unretained(cookies), base::Unretained(event)));
}

class CookiePolicyBrowserTest : public InProcessBrowserTest {
 protected:
  CookiePolicyBrowserTest() {}

  std::string GetCookies(const GURL& url) {
    std::string cookies;
    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    net::URLRequestContextGetter* context_getter =
        browser()->profile()->GetRequestContext();
    EXPECT_TRUE(
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&GetCookiesOnIOThread, url,
                       make_scoped_refptr(context_getter), &event, &cookies)));
    event.Wait();
    return cookies;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookiePolicyBrowserTest);
};

// Visits a page that sets a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, AllowFirstPartyCookies) {
  ASSERT_TRUE(test_server()->Start());

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies,
                                               true);

  GURL url(test_server()->GetURL("set-cookie?cookie1"));

  std::string cookie = GetCookies(url);
  ASSERT_EQ("", cookie);

  ui_test_utils::NavigateToURL(browser(), url);

  cookie = GetCookies(url);
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

  std::string cookie = GetCookies(redirected_url);
  ASSERT_EQ("", cookie);

  host_resolver()->AddRule("www.example.com", "127.0.0.1");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(url.spec() + redirected_url.spec()));

  cookie = GetCookies(redirected_url);
  EXPECT_EQ("cookie2", cookie);
}

}  // namespace

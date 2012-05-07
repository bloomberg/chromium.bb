// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/test/test_server.h"

// Regression test for http://crbug.com/63649.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, RedirectLoopCookies) {
  ASSERT_TRUE(test_server()->Start());

  GURL test_url = test_server()->GetURL("files/redirect-loop.html");

  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), test_url);

  TabContentsWrapper* tab_contents = browser()->GetSelectedTabContentsWrapper();
  ASSERT_EQ(UTF8ToUTF16(test_url.spec() + " failed to load"),
            tab_contents->web_contents()->GetTitle());

  EXPECT_TRUE(tab_contents->content_settings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_COOKIES));
}

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, ContentSettingsBlockDataURLs) {
  GURL url("data:text/html,<title>Data URL</title><script>alert(1)</script>");

  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);

  TabContentsWrapper* tab_contents = browser()->GetSelectedTabContentsWrapper();
  ASSERT_EQ(UTF8ToUTF16("Data URL"), tab_contents->web_contents()->GetTitle());

  EXPECT_TRUE(tab_contents->content_settings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT));
}

// Tests that if redirect across origins occurs, the new process still gets the
// content settings before the resource headers.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, RedirectCrossOrigin) {
  ASSERT_TRUE(test_server()->Start());

  net::HostPortPair host_port = test_server()->host_port_pair();
  DCHECK_EQ(host_port.host(), std::string("127.0.0.1"));

  std::string redirect(base::StringPrintf(
      "http://localhost:%d/files/redirect-cross-origin.html",
      host_port.port()));
  GURL test_url = test_server()->GetURL("server-redirect?" + redirect);

  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), test_url);

  TabContentsWrapper* tab_contents = browser()->GetSelectedTabContentsWrapper();

  EXPECT_TRUE(tab_contents->content_settings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_COOKIES));
}

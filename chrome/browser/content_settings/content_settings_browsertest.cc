// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/test/test_server.h"

// Regression test for http://crbug.com/63649.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, RedirectLoopCookies) {
  ASSERT_TRUE(test_server()->Start());

  GURL test_url = test_server()->GetURL("files/redirect-loop.html");

  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), test_url);

  TabContentsWrapper* tab_contents = browser()->GetSelectedTabContentsWrapper();
  ASSERT_EQ(UTF8ToUTF16(test_url.spec() + " failed to load"),
            tab_contents->tab_contents()->GetTitle());

  EXPECT_TRUE(tab_contents->content_settings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_COOKIES));
}

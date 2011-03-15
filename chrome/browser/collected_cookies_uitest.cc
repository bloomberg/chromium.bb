// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/test/test_server.h"

namespace {

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

}  // namespace

typedef UITest CollectedCookiesTest;

TEST_F(CollectedCookiesTest, DoubleDisplay) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  // Disable cookies.
  ASSERT_TRUE(browser->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                                CONTENT_SETTING_BLOCK));

  // Load a page with cookies.
  ASSERT_TRUE(tab->NavigateToURL(test_server.GetURL("files/cookie1.html")));

  // Click on the info link twice.
  ASSERT_TRUE(tab->ShowCollectedCookiesDialog());
  ASSERT_TRUE(tab->ShowCollectedCookiesDialog());
}

TEST_F(CollectedCookiesTest, NavigateAway) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  // Disable cookies.
  ASSERT_TRUE(browser->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                                CONTENT_SETTING_BLOCK));

  // Load a page with cookies.
  ASSERT_TRUE(tab->NavigateToURL(test_server.GetURL("files/cookie1.html")));

  // Click on the info link.
  ASSERT_TRUE(tab->ShowCollectedCookiesDialog());

  // Navigate to another page.
  ASSERT_TRUE(tab->NavigateToURL(test_server.GetURL("files/cookie2.html")));
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

}  // namespace

typedef UITest CollectedCookiesTest;

// Test is flaky. http://crbug.com/49539
TEST_F(CollectedCookiesTest, FLAKY_DoubleDisplay) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  // Disable cookies.
  ASSERT_TRUE(browser->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                                CONTENT_SETTING_BLOCK));

  // Load a page with cookies.
  ASSERT_TRUE(tab->NavigateToURL(server->TestServerPage("files/cookie1.html")));

  // Click on the info link twice.
  ASSERT_TRUE(tab->ShowCollectedCookiesDialog());
  ASSERT_TRUE(tab->ShowCollectedCookiesDialog());
}

// Test is flaky. http://crbug.com/49539
TEST_F(CollectedCookiesTest, FLAKY_NavigateAway) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  // Disable cookies.
  ASSERT_TRUE(browser->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                                CONTENT_SETTING_BLOCK));

  // Load a page with cookies.
  ASSERT_TRUE(tab->NavigateToURL(server->TestServerPage("files/cookie1.html")));

  // Click on the info link.
  ASSERT_TRUE(tab->ShowCollectedCookiesDialog());

  // Navigate to another page.
  ASSERT_TRUE(tab->NavigateToURL(server->TestServerPage("files/cookie2.html")));
}

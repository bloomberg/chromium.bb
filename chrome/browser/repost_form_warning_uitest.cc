// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/test/test_server.h"

namespace {

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

}  // namespace

typedef UITest RepostFormWarningTest;

#if defined(OS_WIN)
// http://crbug.com/47228
#define MAYBE_TestDoubleReload FLAKY_TestDoubleReload
#else
#define MAYBE_TestDoubleReload TestDoubleReload
#endif

TEST_F(RepostFormWarningTest, MAYBE_TestDoubleReload) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  // Load a form.
  ASSERT_TRUE(tab->NavigateToURL(test_server.GetURL("files/form.html")));
  // Submit it.
  ASSERT_TRUE(tab->NavigateToURL(GURL(
      "javascript:document.getElementById('form').submit()")));

  // Try to reload it twice, checking for repost.
  tab->ReloadAsync();
  tab->ReloadAsync();

  // There should only be one dialog open.
  int num_constrained_windows = 0;
  ASSERT_TRUE(tab->GetConstrainedWindowCount(&num_constrained_windows));
  EXPECT_EQ(1, num_constrained_windows);

  // Navigate away from the page (this is when the test usually crashes).
  ASSERT_TRUE(tab->NavigateToURL(test_server.GetURL("bar")));

  // The dialog should've been closed.
  ASSERT_TRUE(tab->GetConstrainedWindowCount(&num_constrained_windows));
  EXPECT_EQ(0, num_constrained_windows);
}

#if defined(OS_WIN)
// http://crbug.com/47228
#define MAYBE_TestLoginAfterRepost FLAKY_TestLoginAfterRepost
#else
#define MAYBE_TestLoginAfterRepost TestLoginAfterRepost
#endif

TEST_F(RepostFormWarningTest, MAYBE_TestLoginAfterRepost) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  // Load a form.
  ASSERT_TRUE(tab->NavigateToURL(test_server.GetURL("files/form.html")));
  // Submit it.
  ASSERT_TRUE(tab->NavigateToURL(GURL(
      "javascript:document.getElementById('form').submit()")));

  // Try to reload it, checking for repost.
  tab->ReloadAsync();

  // Navigate to a page that requires authentication, bringing up another
  // tab-modal sheet.
  ASSERT_TRUE(tab->NavigateToURL(test_server.GetURL("auth-basic")));

  // Try to reload it again.
  tab->ReloadAsync();

  // Navigate away from the page.
  ASSERT_TRUE(tab->NavigateToURL(test_server.GetURL("bar")));
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

class WebCacheManagerBrowserTest : public InProcessBrowserTest {
};

// Regression test for http://crbug.com/12362.  If a renderer crashes and the
// user navigates to another tab and back, the browser doesn't crash.
IN_PROC_BROWSER_TEST_F(WebCacheManagerBrowserTest, CrashOnceOnly) {
  const base::FilePath kTestDir(FILE_PATH_LITERAL("google"));
  const base::FilePath kTestFile(FILE_PATH_LITERAL("google.html"));
  GURL url(ui_test_utils::GetTestUrl(kTestDir, kTestFile));

  ui_test_utils::NavigateToURL(browser(), url);

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), url);

  WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(tab != NULL);
  base::KillProcess(tab->GetRenderProcessHost()->GetHandle(),
                    content::RESULT_CODE_KILLED, true);

  browser()->tab_strip_model()->ActivateTabAt(0, true);
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->tab_strip_model()->ActivateTabAt(0, true);
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), url);

  // We would have crashed at the above line with the bug.

  browser()->tab_strip_model()->ActivateTabAt(0, true);
  chrome::CloseTab(browser());
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  chrome::CloseTab(browser());
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  chrome::CloseTab(browser());

  ui_test_utils::NavigateToURL(browser(), url);

  // Depending on the speed of execution of the unload event, we may have one or
  // two active renderers at that point (one executing the unload event in
  // background).
  EXPECT_GE(WebCacheManager::GetInstance()->active_renderers_.size(), 1U);
  EXPECT_LE(WebCacheManager::GetInstance()->active_renderers_.size(), 2U);
  EXPECT_EQ(
      WebCacheManager::GetInstance()->inactive_renderers_.size(), 0U);
  EXPECT_GE(WebCacheManager::GetInstance()->stats_.size(), 1U);
  EXPECT_LE(WebCacheManager::GetInstance()->stats_.size(), 2U);
}

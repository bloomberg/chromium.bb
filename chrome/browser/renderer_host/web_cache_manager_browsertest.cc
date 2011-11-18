// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "base/process_util.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/common/result_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebCacheManagerBrowserTest : public InProcessBrowserTest {
};

// Regression test for http://crbug.com/12362.  If a renderer crashes and the
// user navigates to another tab and back, the browser doesn't crash.
// Flaky, http://crbug.com/15288. Disabled, http://crbug.com/69918.
IN_PROC_BROWSER_TEST_F(WebCacheManagerBrowserTest, DISABLED_CrashOnceOnly) {
  const FilePath kTestDir(FILE_PATH_LITERAL("google"));
  const FilePath kTestFile(FILE_PATH_LITERAL("google.html"));
  GURL url(ui_test_utils::GetTestUrl(kTestDir, kTestFile));

  ui_test_utils::NavigateToURL(browser(), url);

  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), url);

  TabContents* tab = browser()->GetTabContentsAt(0);
  ASSERT_TRUE(tab != NULL);
  base::KillProcess(tab->GetRenderProcessHost()->GetHandle(),
                    content::RESULT_CODE_KILLED, true);

  browser()->ActivateTabAt(0, true);
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->ActivateTabAt(0, true);
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), url);

  // We would have crashed at the above line with the bug.

  browser()->ActivateTabAt(0, true);
  browser()->CloseTab();
  browser()->ActivateTabAt(0, true);
  browser()->CloseTab();
  browser()->ActivateTabAt(0, true);
  browser()->CloseTab();

  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(
      WebCacheManager::GetInstance()->active_renderers_.size(), 1U);
  EXPECT_EQ(
      WebCacheManager::GetInstance()->inactive_renderers_.size(), 0U);
  EXPECT_EQ(
      WebCacheManager::GetInstance()->stats_.size(), 1U);
}

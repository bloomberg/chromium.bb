// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const FilePath::CharType* kTestDir = FILE_PATH_LITERAL("popup_blocker");

typedef InProcessBrowserTest PopupBlockerBrowserTest;

#if defined(OS_CHROMEOS)
// Flakily crashes on ChromeOS: http://crbug.com/70192
#define MAYBE_PopupBlockedPostBlank DISABLED_PopupBlockedPostBlank
#else
#define MAYBE_PopupBlockedPostBlank PopupBlockedPostBlank
#endif
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MAYBE_PopupBlockedPostBlank) {
  FilePath file_name(FILE_PATH_LITERAL("popup-blocked-to-post-blank.html"));
  FilePath test_dir(kTestDir);
  GURL url(ui_test_utils::GetTestUrl(test_dir, file_name));
  ui_test_utils::NavigateToURL(browser(), url);

  // If the popup blocker blocked the blank post, there should be only one
  // tab in only one browser window and the URL of current tab must be equal
  // to the original URL.
  EXPECT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(1, browser()->tab_count());
  TabContents* cur_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(cur_tab);
  EXPECT_EQ(url, cur_tab->GetURL());
}

}  // namespace

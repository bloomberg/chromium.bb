// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

namespace content {

class ChromeContentBrowserClientBrowserTest : public InProcessBrowserTest {
 public:
  // Returns the last committed navigation entry of the first tab. May be NULL
  // if there is no such entry.
  NavigationEntry* GetLastCommittedEntry() {
    return chrome::GetTabContentsAt(browser(), 0)->web_contents()
        ->GetController().GetLastCommittedEntry();
  }
};

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest, UberURLHandler) {
  const GURL url1_short(std::string("chrome://settings/"));
  const GURL url1_long(std::string("chrome://chrome/settings/"));

  const GURL url2_short(std::string("chrome://settings/content"));
  const GURL url2_long(std::string("chrome://chrome/settings/content"));

  ui_test_utils::NavigateToURL(browser(), url1_short);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url1_long, entry->GetURL());
  EXPECT_EQ(url1_short, entry->GetVirtualURL());

  ui_test_utils::NavigateToURL(browser(), url2_short);
  entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url2_long, entry->GetURL());
  EXPECT_EQ(url2_short, entry->GetVirtualURL());
}

}  // namespace content

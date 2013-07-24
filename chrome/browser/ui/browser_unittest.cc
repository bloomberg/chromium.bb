// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

using content::SiteInstance;
using content::WebContents;
using content::WebContentsTester;

class BrowserUnitTest : public BrowserWithTestWindowTest {
 public:
  BrowserUnitTest() {}
  virtual ~BrowserUnitTest() {}

  // Caller owns the memory.
  WebContents* CreateTestWebContents() {
    return WebContentsTester::CreateTestWebContents(
        profile(), SiteInstance::Create(profile()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserUnitTest);
};

// Don't build on platforms without a tab strip.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Ensure crashed tabs are not reloaded when selected. crbug.com/232323
TEST_F(BrowserUnitTest, ReloadCrashedTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Start with a single foreground tab. |tab_strip_model| owns the memory.
  WebContents* contents1 = CreateTestWebContents();
  tab_strip_model->AppendWebContents(contents1, true);
  WebContentsTester::For(contents1)->NavigateAndCommit(GURL("about:blank"));
  WebContentsTester::For(contents1)->TestSetIsLoading(false);
  EXPECT_TRUE(tab_strip_model->IsTabSelected(0));
  EXPECT_FALSE(contents1->IsLoading());

  // Add a second tab in the background.
  WebContents* contents2 = CreateTestWebContents();
  tab_strip_model->AppendWebContents(contents2, false);
  WebContentsTester::For(contents2)->NavigateAndCommit(GURL("about:blank"));
  WebContentsTester::For(contents2)->TestSetIsLoading(false);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(tab_strip_model->IsTabSelected(0));
  EXPECT_FALSE(contents2->IsLoading());

  // Simulate the second tab crashing.
  contents2->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_TRUE(contents2->IsCrashed());

  // Selecting the second tab does not cause a load or clear the crash.
  tab_strip_model->ActivateTabAt(1, true);
  EXPECT_TRUE(tab_strip_model->IsTabSelected(1));
  EXPECT_FALSE(contents2->IsLoading());
  EXPECT_TRUE(contents2->IsCrashed());
}

#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

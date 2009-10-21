// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/gtk/view_id_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"

class ViewIDTest : public InProcessBrowserTest {
 public:
  ViewIDTest() : root_window_(NULL) {}

  void CheckViewID(ViewID id, bool should_have) {
    if (!root_window_)
      root_window_ = GTK_WIDGET(browser()->window()->GetNativeHandle());

    ASSERT_TRUE(root_window_);
    EXPECT_EQ(should_have, !!ViewIDUtil::GetWidget(root_window_, id));
  }

 private:
  GtkWidget* root_window_;
};

IN_PROC_BROWSER_TEST_F(ViewIDTest, Basic) {
  for (int i = VIEW_ID_TOOLBAR; i < VIEW_ID_PREDEFINED_COUNT; ++i) {
    // http://crbug.com/21152
    if (i == VIEW_ID_BOOKMARK_MENU)
      continue;

    // Extension shelf is being removed, http://crbug.com/25106.
    if (i == VIEW_ID_DEV_EXTENSION_SHELF)
      continue;

    CheckViewID(static_cast<ViewID>(i), true);
  }

  CheckViewID(VIEW_ID_PREDEFINED_COUNT, false);
}

IN_PROC_BROWSER_TEST_F(ViewIDTest, Delegate) {
  CheckViewID(VIEW_ID_TAB_0, true);
  CheckViewID(VIEW_ID_TAB_1, false);

  browser()->OpenURL(GURL(chrome::kAboutBlankURL), GURL(),
                     NEW_BACKGROUND_TAB, PageTransition::TYPED);

  CheckViewID(VIEW_ID_TAB_0, true);
  CheckViewID(VIEW_ID_TAB_1, true);
}

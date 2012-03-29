// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"

using content::OpenURLParams;

class ViewIDTest : public InProcessBrowserTest {
 public:
  ViewIDTest() : root_window_(NULL) {}

  void CheckViewID(ViewID id, bool should_have) {
    if (!root_window_)
      root_window_ = GTK_WIDGET(browser()->window()->GetNativeHandle());

    ASSERT_TRUE(root_window_);
    EXPECT_EQ(should_have, !!ViewIDUtil::GetWidget(root_window_, id))
        << " Failed id=" << id;
  }

 private:
  GtkWidget* root_window_;
};

IN_PROC_BROWSER_TEST_F(ViewIDTest, Basic) {
  // Make sure FindBar is created to test
  // VIEW_ID_FIND_IN_PAGE_TEXT_FIELD and VIEW_ID_FIND_IN_PAGE.
  browser()->ShowFindBar();

  for (int i = VIEW_ID_TOOLBAR; i < VIEW_ID_PREDEFINED_COUNT; ++i) {
    // The following ids are used only in views implementation.
    if (i == VIEW_ID_CONTENTS_SPLIT ||
        i == VIEW_ID_INFO_BAR_CONTAINER ||
        i == VIEW_ID_DEV_TOOLS_DOCKED ||
        i == VIEW_ID_DOWNLOAD_SHELF ||
        i == VIEW_ID_BOOKMARK_BAR_ELEMENT ||
        i == VIEW_ID_TAB ||
        i == VIEW_ID_FEEDBACK_BUTTON ||
        i == VIEW_ID_OMNIBOX) {
      continue;
    }

    // Chrome To Mobile is disabled by default.
    if (i == VIEW_ID_CHROME_TO_MOBILE_BUTTON)
      continue;

    CheckViewID(static_cast<ViewID>(i), true);
  }

  CheckViewID(VIEW_ID_PREDEFINED_COUNT, false);
}

IN_PROC_BROWSER_TEST_F(ViewIDTest, Delegate) {
  CheckViewID(VIEW_ID_TAB_0, true);
  CheckViewID(VIEW_ID_TAB_1, false);

  browser()->OpenURL(OpenURLParams(GURL(chrome::kAboutBlankURL),
                     content::Referrer(),
                     NEW_BACKGROUND_TAB, content::PAGE_TRANSITION_TYPED,
                     false));

  CheckViewID(VIEW_ID_TAB_0, true);
  CheckViewID(VIEW_ID_TAB_1, true);
}

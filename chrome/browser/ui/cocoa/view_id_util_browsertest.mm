// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

// Basic sanity check of ViewID use on the mac.
class ViewIDTest : public InProcessBrowserTest {
 public:
  ViewIDTest() : root_window_(nil) {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalExtensionApis);
  }

  void CheckViewID(ViewID view_id, bool should_have) {
    if (!root_window_)
      root_window_ = browser()->window()->GetNativeHandle();

    ASSERT_TRUE(root_window_);
    NSView* view = view_id_util::GetView(root_window_, view_id);
    EXPECT_EQ(should_have, !!view) << " Failed id=" << view_id;
  }

  void DoTest() {
    // Make sure FindBar is created to test
    // VIEW_ID_FIND_IN_PAGE_TEXT_FIELD and VIEW_ID_FIND_IN_PAGE.
    browser()->ShowFindBar();

    // Make sure sidebar is created to test VIEW_ID_SIDE_BAR_CONTAINER.
    const char sidebar_content_id[] = "test_content_id";
    SidebarManager::GetInstance()->ShowSidebar(
        browser()->GetSelectedTabContents(), sidebar_content_id);
    SidebarManager::GetInstance()->ExpandSidebar(
        browser()->GetSelectedTabContents(), sidebar_content_id);

    // Make sure docked devtools is created to test VIEW_ID_DEV_TOOLS_DOCKED
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kDevToolsOpenDocked,
                                                 true);
    browser()->ToggleDevToolsWindow(DEVTOOLS_TOGGLE_ACTION_INSPECT);

    // Make sure download shelf is created to test VIEW_ID_DOWNLOAD_SHELF
    browser()->window()->GetDownloadShelf()->Show();

    // Create a bookmark to test VIEW_ID_BOOKMARK_BAR_ELEMENT
    BookmarkModel* bookmark_model = browser()->profile()->GetBookmarkModel();
    if (bookmark_model) {
      if (!bookmark_model->IsLoaded())
        ui_test_utils::WaitForBookmarkModelToLoad(bookmark_model);

      bookmark_model->SetURLStarred(GURL(chrome::kAboutBlankURL),
                                    UTF8ToUTF16("about"), true);
    }

    for (int i = VIEW_ID_TOOLBAR; i < VIEW_ID_PREDEFINED_COUNT; ++i) {
      // Mac implementation does not support following ids yet.
      if (i == VIEW_ID_STAR_BUTTON ||
          i == VIEW_ID_AUTOCOMPLETE ||
          i == VIEW_ID_CONTENTS_SPLIT ||
          i == VIEW_ID_SIDE_BAR_SPLIT ||
          i == VIEW_ID_FEEDBACK_BUTTON) {
        continue;
      }

      CheckViewID(static_cast<ViewID>(i), true);
    }

    CheckViewID(VIEW_ID_TAB, true);
    CheckViewID(VIEW_ID_TAB_STRIP, true);
    CheckViewID(VIEW_ID_PREDEFINED_COUNT, false);
  }

 private:
  NSWindow* root_window_;
};

IN_PROC_BROWSER_TEST_F(ViewIDTest, Basic) {
  ASSERT_NO_FATAL_FAILURE(DoTest());
}

IN_PROC_BROWSER_TEST_F(ViewIDTest, Fullscreen) {
  browser()->window()->SetFullscreen(true);
  ASSERT_NO_FATAL_FAILURE(DoTest());
}

IN_PROC_BROWSER_TEST_F(ViewIDTest, Tab) {
  CheckViewID(VIEW_ID_TAB_0, true);
  CheckViewID(VIEW_ID_TAB_LAST, true);

  // Open 9 new tabs.
  for (int i = 1; i <= 9; ++i) {
    CheckViewID(static_cast<ViewID>(VIEW_ID_TAB_0 + i), false);
    browser()->OpenURL(GURL(chrome::kAboutBlankURL), GURL(),
                       NEW_BACKGROUND_TAB, PageTransition::TYPED);
    CheckViewID(static_cast<ViewID>(VIEW_ID_TAB_0 + i), true);
    // VIEW_ID_TAB_LAST should always be available.
    CheckViewID(VIEW_ID_TAB_LAST, true);
  }

  // Open the 11th tab.
  browser()->OpenURL(GURL(chrome::kAboutBlankURL), GURL(),
                     NEW_BACKGROUND_TAB, PageTransition::TYPED);
  CheckViewID(VIEW_ID_TAB_LAST, true);
}

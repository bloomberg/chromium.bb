// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

class ActionBoxTest : public InProcessBrowserTest,
                      public content::NotificationObserver {
 protected:
  ActionBoxTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    ASSERT_NO_FATAL_FAILURE(SetupComponents());
    chrome::FocusLocationBar(browser());
    // Use Textfield's view id on pure views. See crbug.com/71144.
    ViewID location_bar_focus_view_id = VIEW_ID_LOCATION_BAR;
#if defined(USE_AURA)
    location_bar_focus_view_id = VIEW_ID_OMNIBOX;
#endif
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                             location_bar_focus_view_id));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Enable Action Box UI for the test.
    command_line->AppendSwitchASCII(switches::kActionBox, "1");
  }

  void SetupComponents() {}

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case content::NOTIFICATION_WEB_CONTENTS_DESTROYED:
      case chrome::NOTIFICATION_TAB_PARENTED:
      case chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY:
      case chrome::NOTIFICATION_HISTORY_LOADED:
      case chrome::NOTIFICATION_HISTORY_URLS_MODIFIED:
      case chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED:
        break;
      default:
        FAIL() << "Unexpected notification type";
    }
    MessageLoop::current()->Quit();
  }

  DISALLOW_COPY_AND_ASSIGN(ActionBoxTest);
};

// Test if Bookmark star appears after bookmarking a page in the action box, and
// disappears after unbookmarking a page.
IN_PROC_BROWSER_TEST_F(ActionBoxTest, BookmarkAPageTest) {
  LocationBarTesting* loc_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();

  // Navigate somewhere we can bookmark.
  ui_test_utils::NavigateToURL(browser(), GURL("http://www.google.com"));

  // Make sure the bookmarking system is up and running.
  BookmarkModel* model =
      BookmarkModelFactory::GetForProfile(browser()->profile());
  ui_test_utils::WaitForBookmarkModelToLoad(model);

  // Page is not bookmarked yet.
  ASSERT_FALSE(loc_bar->GetBookmarkStarVisibility());

  // Simulate an action box click and menu item selection.
  chrome::ExecuteCommand(browser(), IDC_BOOKMARK_PAGE_FROM_STAR);

  // Page is now bookmarked.
  ASSERT_TRUE(loc_bar->GetBookmarkStarVisibility());

  // Get the BookmarkModel to unbookmark the bookmark.
  bookmark_utils::RemoveAllBookmarks(model, GURL("http://www.google.com"));

  // Page is now unbookmarked.
  ASSERT_FALSE(loc_bar->GetBookmarkStarVisibility());

}

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

class ToolbarViewTest : public InProcessBrowserTest {
 public:
  ToolbarViewTest() {}

  void RunToolbarCycleFocusTest(Browser* browser);

 private:
  DISALLOW_COPY_AND_ASSIGN(ToolbarViewTest);
};

void ToolbarViewTest::RunToolbarCycleFocusTest(Browser* browser) {
  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  views::FocusManager* focus_manager = widget->GetFocusManager();
  CommandUpdater* updater = browser->command_controller()->command_updater();

  // Send focus to the toolbar as if the user pressed Alt+Shift+T.
  updater->ExecuteCommand(IDC_FOCUS_TOOLBAR);

  views::View* first_view = focus_manager->GetFocusedView();
  std::vector<int> ids;

  // Press Tab to cycle through all of the controls in the toolbar until
  // we end up back where we started.
  bool found_reload = false;
  bool found_location_bar = false;
  bool found_app_menu = false;
  const views::View* view = NULL;
  while (view != first_view) {
    focus_manager->AdvanceFocus(false);
    view = focus_manager->GetFocusedView();
    ids.push_back(view->id());
    if (view->id() == VIEW_ID_RELOAD_BUTTON)
      found_reload = true;
    if (view->id() == VIEW_ID_APP_MENU)
      found_app_menu = true;
    if (view->id() == VIEW_ID_OMNIBOX)
      found_location_bar = true;
    if (ids.size() > 100)
      GTEST_FAIL() << "Tabbed 100 times, still haven't cycled back!";
  }

  // Make sure we found a few key items.
  ASSERT_TRUE(found_reload);
  ASSERT_TRUE(found_app_menu);
  ASSERT_TRUE(found_location_bar);

  // Now press Shift-Tab to cycle backwards.
  std::vector<int> reverse_ids;
  view = NULL;
  while (view != first_view) {
    focus_manager->AdvanceFocus(true);
    view = focus_manager->GetFocusedView();
    reverse_ids.push_back(view->id());
    if (reverse_ids.size() > 100)
      GTEST_FAIL() << "Tabbed 100 times, still haven't cycled back!";
  }

  // Assert that the views were focused in exactly the reverse order.
  // The sequences should be the same length, and the last element will
  // be the same, and the others are reverse.
  ASSERT_EQ(ids.size(), reverse_ids.size());
  size_t count = ids.size();
  for (size_t i = 0; i < count - 1; i++)
    EXPECT_EQ(ids[i], reverse_ids[count - 2 - i]);
}

// The test is flaky on Win (http://crbug.com/152938) and crashes on CrOS under
// AddressSanitizer (http://crbug.com/154657).
IN_PROC_BROWSER_TEST_F(ToolbarViewTest, DISABLED_ToolbarCycleFocus) {
  RunToolbarCycleFocusTest(browser());
}

#if defined(OS_WIN)
// http://crbug.com/152938 Flaky on win.
#define MAYBE_ToolbarCycleFocusWithBookmarkBar \
    DISABLED_ToolbarCycleFocusWithBookmarkBar
#else
#define MAYBE_ToolbarCycleFocusWithBookmarkBar ToolbarCycleFocusWithBookmarkBar
#endif
IN_PROC_BROWSER_TEST_F(ToolbarViewTest,
                       MAYBE_ToolbarCycleFocusWithBookmarkBar) {
  CommandUpdater* updater = browser()->command_controller()->command_updater();
  updater->ExecuteCommand(IDC_SHOW_BOOKMARK_BAR);

  BookmarkModel* model =
      BookmarkModelFactory::GetForProfile(browser()->profile());
  bookmarks::AddIfNotBookmarked(
      model, GURL("http://foo.com"), base::ASCIIToUTF16("Foo"));

  // We want to specifically test the case where the bookmark bar is
  // already showing when a window opens, so create a second browser
  // window with the same profile.
  Browser* second_browser = CreateBrowser(browser()->profile());
  RunToolbarCycleFocusTest(second_browser);
}

}  // namespace

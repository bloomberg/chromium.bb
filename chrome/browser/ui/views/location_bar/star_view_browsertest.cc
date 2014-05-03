// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

namespace {

typedef InProcessBrowserTest StarViewTest;

// Verify that clicking the bookmark star a second time hides the bookmark
// bubble.
#if defined(OS_LINUX) && defined(USE_AURA) && !defined(OS_CHROMEOS)
#define MAYBE_HideOnSecondClick DISABLED_HideOnSecondClick
#else
#define MAYBE_HideOnSecondClick HideOnSecondClick
#endif
IN_PROC_BROWSER_TEST_F(StarViewTest, MAYBE_HideOnSecondClick) {
  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  views::View* star_view =
      browser_view->GetToolbarView()->location_bar()->star_view();

  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                gfx::Point(), ui::EF_LEFT_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON);

  // Verify that clicking once shows the bookmark bubble.
  EXPECT_FALSE(BookmarkBubbleView::IsShowing());
  star_view->OnMousePressed(pressed_event);
  EXPECT_FALSE(BookmarkBubbleView::IsShowing());
  star_view->OnMouseReleased(released_event);
  EXPECT_TRUE(BookmarkBubbleView::IsShowing());

  // Verify that clicking again doesn't reshow it.
  star_view->OnMousePressed(pressed_event);
  // Hide the bubble manually. In the browser this would normally happen during
  // the event processing.
  BookmarkBubbleView::Hide();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(BookmarkBubbleView::IsShowing());
  star_view->OnMouseReleased(released_event);
  EXPECT_FALSE(BookmarkBubbleView::IsShowing());
}

#if defined(OS_WIN)

class StarViewTestNoDWM : public InProcessBrowserTest {
 public:
  StarViewTestNoDWM() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kDisableDwmComposition);
  }
};

BOOL CALLBACK EnumerateChildren(HWND hwnd, LPARAM l_param) {
  HWND* child = reinterpret_cast<HWND*>(l_param);
  *child = hwnd;
  // The first child window is the plugin, then its children. So stop
  // enumerating after the first callback.
  return FALSE;
}

// Ensure that UIs like the star window, user profiler picker, omnibox
// popup and bookmark editor are always over a windowed NPAPI plugin even if
// kDisableDwmComposition is used.
IN_PROC_BROWSER_TEST_F(StarViewTestNoDWM, WindowedNPAPIPluginHidden) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysAuthorize,
                                               true);

  // First switch to a new tab and back, to also test a scenario where we
  // stopped watching the root window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  browser()->tab_strip_model()->ActivateTabAt(0, true);

  // First load the page and wait for the NPAPI plugin's window to display.
  base::string16 expected_title(base::ASCIIToUTF16("ready"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(tab, expected_title);

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("printing"),
      base::FilePath().AppendASCII("npapi_plugin.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Now get the region of the plugin before the star view is shown.
  HWND hwnd = tab->GetNativeView()->GetHost()->GetAcceleratedWidget();
  HWND child = NULL;
  EnumChildWindows(hwnd, EnumerateChildren,reinterpret_cast<LPARAM>(&child));

  RECT region_before, region_after;
  int result = GetWindowRgnBox(child, &region_before);
  ASSERT_EQ(result, SIMPLEREGION);

  // Now show the star view
  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  views::ImageView* star_view =
      browser_view->GetToolbarView()->location_bar()->star_view();

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  // Verify that clicking once shows the bookmark bubble.
  ui_test_utils::MoveMouseToCenterAndPress(
      star_view,
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      runner->QuitClosure());
  runner->Run();

  EXPECT_TRUE(BookmarkBubbleView::IsShowing());

  result = GetWindowRgnBox(child, &region_after);
  if (result == NULLREGION) {
    // Depending on the browser window size, the plugin could be full covered.
    return;
  }

  if (result == COMPLEXREGION) {
    // Complex region, by definition not equal to the initial region.
    return;
  }

  ASSERT_EQ(result, SIMPLEREGION);
  bool rects_equal =
      region_before.left == region_after.left &&
      region_before.top == region_after.top &&
      region_before.right == region_after.right &&
      region_before.bottom == region_after.bottom;
  ASSERT_FALSE(rects_equal);
}

#endif

}  // namespace

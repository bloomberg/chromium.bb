// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/app_modal_dialog.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/test/ui_controls.h"

namespace {

class MouseLeaveTest : public InProcessBrowserTest {
 public:
  MouseLeaveTest() {}

  void LoadTestPageAndWaitForMouseOver(content::WebContents* tab) {
    gfx::Rect tab_view_bounds = tab->GetContainerBounds();
    GURL test_url = ui_test_utils::GetTestUrl(
        base::FilePath(), base::FilePath(FILE_PATH_LITERAL("mouseleave.html")));

    gfx::Point in_content(tab_view_bounds.x() + tab_view_bounds.width() / 2,
                          tab_view_bounds.y() + 10);
    out_of_content_ =
        gfx::Point(tab_view_bounds.x() + tab_view_bounds.width() / 2,
                   tab_view_bounds.y() - 2);

    // Start by moving the point just above the content.
    ui_controls::SendMouseMove(out_of_content_.x(), out_of_content_.y());

    // Navigate to the test html page.
    base::string16 load_expected_title(base::ASCIIToUTF16("onload"));
    content::TitleWatcher load_title_watcher(tab, load_expected_title);
    ui_test_utils::NavigateToURL(browser(), test_url);
    // Wait for the onload() handler to complete so we can do the
    // next part of the test.
    EXPECT_EQ(load_expected_title, load_title_watcher.WaitAndGetTitle());

    // Move the cursor to the top-center of the content which will trigger
    // a javascript onMouseOver event.
    ui_controls::SendMouseMove(in_content.x(), in_content.y());

    // Wait on the correct intermediate title.
    base::string16 entered_expected_title(base::ASCIIToUTF16("entered"));
    content::TitleWatcher entered_title_watcher(tab, entered_expected_title);
    EXPECT_EQ(entered_expected_title, entered_title_watcher.WaitAndGetTitle());
  }

  void MouseLeaveTestCommon() {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    EXPECT_NO_FATAL_FAILURE(LoadTestPageAndWaitForMouseOver(tab));

    // Move the cursor above the content again, which should trigger
    // a javascript onMouseOut event.
    ui_controls::SendMouseMove(out_of_content_.x(), out_of_content_.y());

    // Wait on the correct final value of the cookie.
    base::string16 left_expected_title(base::ASCIIToUTF16("left"));
    content::TitleWatcher left_title_watcher(tab, left_expected_title);
    EXPECT_EQ(left_expected_title, left_title_watcher.WaitAndGetTitle());
  }

  // The coordinates out of the content to move the mouse point
  gfx::Point out_of_content_;

  DISALLOW_COPY_AND_ASSIGN(MouseLeaveTest);
};

#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_WIN)
// OS_MACOSX: Missing automation provider support: http://crbug.com/45892.
// OS_LINUX: http://crbug.com/133361.
// OS_WIN: http://crbug.com/419468
#define MAYBE_TestOnMouseOut DISABLED_TestOnMouseOut
#else
#define MAYBE_TestOnMouseOut TestOnMouseOut
#endif

IN_PROC_BROWSER_TEST_F(MouseLeaveTest, MAYBE_TestOnMouseOut) {
  MouseLeaveTestCommon();
}

#if defined(OS_WIN)
// For MAC: Missing automation provider support: http://crbug.com/45892
// For linux : http://crbug.com/133361. interactive mouse tests are flaky.
IN_PROC_BROWSER_TEST_F(MouseLeaveTest, MouseDownOnBrowserCaption) {
  gfx::Rect browser_bounds = browser()->window()->GetBounds();
  ui_controls::SendMouseMove(browser_bounds.x() + 200,
                             browser_bounds.y() + 10);
  ui_controls::SendMouseClick(ui_controls::LEFT);

  MouseLeaveTestCommon();
}
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
// Test that a mouseleave is not triggered when showing the context menu.
// If the test is failed, it means that Blink gets the mouseleave event
// when showing the context menu and it could make the unexpecting
// content behavior such as clearing the hover status.
// Please refer to the below issue for understanding what happens .
// TODO: Make test pass on OS_WIN and OS_MACOSX
// OS_WIN: Flaky. See http://crbug.com/656101.
// OS_MACOSX: Missing automation provider support: http://crbug.com/45892.
#define MAYBE_ContextMenu DISABLED_ContextMenu
#else
#define MAYBE_ContextMenu ContextMenu
#endif

IN_PROC_BROWSER_TEST_F(MouseLeaveTest, MAYBE_ContextMenu) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_NO_FATAL_FAILURE(LoadTestPageAndWaitForMouseOver(tab));

  ContextMenuWaiter menu_observer(content::NotificationService::AllSources());
  ui_controls::SendMouseClick(ui_controls::RIGHT);
  // Wait until the context menu is opened and closed.
  menu_observer.WaitForMenuOpenAndClose();

  tab->GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16("done()"));
  const base::string16 success_title = base::ASCIIToUTF16("without mouseleave");
  const base::string16 failure_title = base::ASCIIToUTF16("with mouseleave");
  content::TitleWatcher done_title_watcher(tab, success_title);
  done_title_watcher.AlsoWaitForTitle(failure_title);

  EXPECT_EQ(success_title, done_title_watcher.WaitAndGetTitle());
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Test that a mouseleave is not triggered when showing a modal dialog.
// Sample regression: crbug.com/394672
// TODO: Make test pass on OS_WIN and OS_MACOSX
// OS_WIN: http://crbug.com/450138
// OS_MACOSX: Missing automation provider support: http://crbug.com/45892.
#define MAYBE_ModalDialog DISABLED_ModalDialog
#else
#define MAYBE_ModalDialog ModalDialog
#endif

IN_PROC_BROWSER_TEST_F(MouseLeaveTest, MAYBE_ModalDialog) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_NO_FATAL_FAILURE(LoadTestPageAndWaitForMouseOver(tab));

  tab->GetMainFrame()->ExecuteJavaScriptForTests(base::UTF8ToUTF16("alert()"));
  app_modal::AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  // Cancel the dialog.
  alert->CloseModalDialog();

  tab->GetMainFrame()->ExecuteJavaScriptForTests(base::ASCIIToUTF16("done()"));
  const base::string16 success_title = base::ASCIIToUTF16("without mouseleave");
  const base::string16 failure_title = base::ASCIIToUTF16("with mouseleave");
  content::TitleWatcher done_title_watcher(tab, success_title);
  done_title_watcher.AlsoWaitForTitle(failure_title);
  EXPECT_EQ(success_title, done_title_watcher.WaitAndGetTitle());
}

}  // namespace

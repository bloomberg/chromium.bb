// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/test/ui_controls.h"

namespace {

class MouseLeaveTest : public InProcessBrowserTest {
 public:
  MouseLeaveTest() {}

  void MouseLeaveTestCommon() {
    GURL test_url = ui_test_utils::GetTestUrl(
        base::FilePath(), base::FilePath(FILE_PATH_LITERAL("mouseleave.html")));

    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    gfx::Rect tab_view_bounds = tab->GetContainerBounds();

    gfx::Point in_content_point(
        tab_view_bounds.x() + tab_view_bounds.width() / 2,
        tab_view_bounds.y() + 10);
    gfx::Point above_content_point(
        tab_view_bounds.x() + tab_view_bounds.width() / 2,
        tab_view_bounds.y() - 2);

    // Start by moving the point just above the content.
    ui_controls::SendMouseMove(above_content_point.x(),
                               above_content_point.y());

    // Navigate to the test html page.
    base::string16 load_expected_title(base::ASCIIToUTF16("onload"));
    content::TitleWatcher load_title_watcher(tab, load_expected_title);
    ui_test_utils::NavigateToURL(browser(), test_url);
    // Wait for the onload() handler to complete so we can do the
    // next part of the test.
    EXPECT_EQ(load_expected_title, load_title_watcher.WaitAndGetTitle());

    // Move the cursor to the top-center of the content, which will trigger
    // a javascript onMouseOver event.
    ui_controls::SendMouseMove(in_content_point.x(), in_content_point.y());

    // Wait on the correct intermediate title.
    base::string16 entered_expected_title(base::ASCIIToUTF16("entered"));
    content::TitleWatcher entered_title_watcher(tab, entered_expected_title);
    EXPECT_EQ(entered_expected_title, entered_title_watcher.WaitAndGetTitle());

    // Move the cursor above the content again, which should trigger
    // a javascript onMouseOut event.
    ui_controls::SendMouseMove(above_content_point.x(),
                               above_content_point.y());

    // Wait on the correct final value of the cookie.
    base::string16 left_expected_title(base::ASCIIToUTF16("left"));
    content::TitleWatcher left_title_watcher(tab, left_expected_title);
    EXPECT_EQ(left_expected_title, left_title_watcher.WaitAndGetTitle());
  }

  DISALLOW_COPY_AND_ASSIGN(MouseLeaveTest);
};

#if defined(OS_MACOSX)
// Missing automation provider support: http://crbug.com/45892
#define MAYBE_TestOnMouseOut DISABLED_TestOnMouseOut
#elif defined(OS_LINUX)
// http://crbug.com/133361
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

}  // namespace

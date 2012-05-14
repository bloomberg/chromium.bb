// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/ui_controls/ui_controls.h"

namespace {

class MouseLeaveTest : public InProcessBrowserTest {
 public:
  MouseLeaveTest() {}

  DISALLOW_COPY_AND_ASSIGN(MouseLeaveTest);
};

#if defined(OS_MACOSX)
// Missing automation provider support: http://crbug.com/45892
#define MAYBE_TestOnMouseOut DISABLED_TestOnMouseOut
#else
#define MAYBE_TestOnMouseOut TestOnMouseOut
#endif

IN_PROC_BROWSER_TEST_F(MouseLeaveTest, MAYBE_TestOnMouseOut) {
  GURL test_url = ui_test_utils::GetTestUrl(
      FilePath(), FilePath(FILE_PATH_LITERAL("mouseleave.html")));

  content::WebContents* tab = browser()->GetSelectedWebContents();
  gfx::Rect tab_view_bounds;
  tab->GetView()->GetContainerBounds(&tab_view_bounds);

  gfx::Point in_content_point(
      tab_view_bounds.x() + tab_view_bounds.width() / 2,
      tab_view_bounds.y() + 10);
  gfx::Point above_content_point(
      tab_view_bounds.x() + tab_view_bounds.width() / 2,
      tab_view_bounds.y() - 2);

  // Start by moving the point just above the content.
  ui_controls::SendMouseMove(above_content_point.x(), above_content_point.y());

  // Navigate to the test html page.
  string16 load_expected_title(ASCIIToUTF16("onload"));
  ui_test_utils::TitleWatcher load_title_watcher(tab, load_expected_title);
  ui_test_utils::NavigateToURL(browser(), test_url);
  // Wait for the onload() handler to complete so we can do the
  // next part of the test.
  EXPECT_EQ(load_expected_title, load_title_watcher.WaitAndGetTitle());

  // Move the cursor to the top-center of the content, which will trigger
  // a javascript onMouseOver event.
  ui_controls::SendMouseMove(in_content_point.x(), in_content_point.y());

  // Wait on the correct intermediate title.
  string16 entered_expected_title(ASCIIToUTF16("entered"));
  ui_test_utils::TitleWatcher entered_title_watcher(
      tab, entered_expected_title);
  EXPECT_EQ(entered_expected_title, entered_title_watcher.WaitAndGetTitle());

  // Move the cursor above the content again, which should trigger
  // a javascript onMouseOut event.
  ui_controls::SendMouseMove(above_content_point.x(), above_content_point.y());

  // Wait on the correct final value of the cookie.
  string16 left_expected_title(ASCIIToUTF16("left"));
  ui_test_utils::TitleWatcher left_title_watcher(tab, left_expected_title);
  EXPECT_EQ(left_expected_title, left_title_watcher.WaitAndGetTitle());
}

}  // namespace

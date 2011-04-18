// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/test/test_server.h"

namespace {

const char kSimplePage[] = "404_is_enough_for_us.html";

void OnClicked(GtkWidget* widget, bool* clicked_bit) {
  *clicked_bit = true;
}

}  // namespace

class BookmarkBarGtkInteractiveUITest : public InProcessBrowserTest {
};

// Makes sure that when you switch back to an NTP with an active findbar,
// the findbar is above the floating bookmark bar.
IN_PROC_BROWSER_TEST_F(BookmarkBarGtkInteractiveUITest, FindBarTest) {
  ASSERT_TRUE(test_server()->Start());

  // Create new tab; open findbar.
  browser()->NewTab();
  browser()->Find();

  // Create new tab with an arbitrary URL.
  GURL url = test_server()->GetURL(kSimplePage);
  browser()->AddSelectedTabWithURL(url, PageTransition::TYPED);

  // Switch back to the NTP with the active findbar.
  browser()->ActivateTabAt(1, false);

  // Wait for the findbar to show.
  MessageLoop::current()->RunAllPending();

  // Set focus somewhere else, so that we can test clicking on the findbar
  // works.
  browser()->FocusLocationBar();
  ui_test_utils::ClickOnView(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD);
  ui_test_utils::IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD);
}

// Makes sure that you can click on the floating bookmark bar.
IN_PROC_BROWSER_TEST_F(BookmarkBarGtkInteractiveUITest, ClickOnFloatingTest) {
  ASSERT_TRUE(test_server()->Start());

  GtkWidget* other_bookmarks =
      ViewIDUtil::GetWidget(GTK_WIDGET(browser()->window()->GetNativeHandle()),
      VIEW_ID_OTHER_BOOKMARKS);
  bool has_been_clicked = false;
  g_signal_connect(other_bookmarks, "clicked",
                   G_CALLBACK(OnClicked), &has_been_clicked);

  // Create new tab.
  browser()->NewTab();

  // Wait for the floating bar to appear.
  MessageLoop::current()->RunAllPending();

  // This is kind of a hack. Calling this just once doesn't seem to send a click
  // event, but doing it twice works.
  // http://crbug.com/35581
  ui_test_utils::ClickOnView(browser(), VIEW_ID_OTHER_BOOKMARKS);
  ui_test_utils::ClickOnView(browser(), VIEW_ID_OTHER_BOOKMARKS);

  EXPECT_TRUE(has_been_clicked);
}

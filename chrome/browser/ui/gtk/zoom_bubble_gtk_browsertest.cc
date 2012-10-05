// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/string_number_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/browser/ui/gtk/zoom_bubble_gtk.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(dbeam): share some testing code with LocationBarViewGtkZoomTest.

namespace {

int GetZoomPercent(content::WebContents* contents) {
  bool dummy;
  return contents->GetZoomPercent(&dummy, &dummy);
}

void ExpectZoomedIn(content::WebContents* contents) {
  EXPECT_GT(GetZoomPercent(contents), 100);
}

void ExpectZoomedOut(content::WebContents* contents) {
  EXPECT_LT(GetZoomPercent(contents), 100);
}

void ExpectAtDefaultZoom(content::WebContents* contents) {
  EXPECT_EQ(GetZoomPercent(contents), 100);
}

}

class ZoomBubbleGtkTest : public InProcessBrowserTest {
 public:
  ZoomBubbleGtkTest() {}
  virtual ~ZoomBubbleGtkTest() {}

 protected:
  ZoomBubbleGtk* GetZoomBubble() {
    return ZoomBubbleGtk::g_bubble;
  }

  bool ZoomBubbleIsShowing() {
    return ZoomBubbleGtk::IsShowing();
  }

  void ExpectLabelTextContainsZoom() {
    std::string label(gtk_label_get_text(GTK_LABEL(GetZoomBubble()->label_)));
    content::WebContents* contents = chrome::GetActiveWebContents(browser());
    std::string zoom_percent = base::IntToString(GetZoomPercent(contents));
    EXPECT_FALSE(label.find(zoom_percent) == std::string::npos);
  }

  void ResetZoom() {
    WaitForZoom(content::PAGE_ZOOM_RESET);
  }

  content::WebContents* SetUpTest() {
    TearDown();
    content::WebContents* contents = chrome::GetActiveWebContents(browser());
    ResetZoom();
    ExpectAtDefaultZoom(contents);
    return contents;
  }

  void TearDown() {
    ZoomBubbleGtk::Close();
  }

  void ZoomIn() {
    WaitForZoom(content::PAGE_ZOOM_IN);
  }

  void ZoomOut() {
    WaitForZoom(content::PAGE_ZOOM_OUT);
  }

 private:
  void WaitForZoom(content::PageZoom zoom_action) {
    content::WindowedNotificationObserver zoom_observer(
        content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
        content::NotificationService::AllSources());
    chrome::Zoom(browser(), zoom_action);
    zoom_observer.Wait();
  }

  DISALLOW_COPY_AND_ASSIGN(ZoomBubbleGtkTest);
};

IN_PROC_BROWSER_TEST_F(ZoomBubbleGtkTest, BubbleSanityTest) {
  ResetZoom();

  // The bubble assumes it shows only at non-default zoom levels.
  ZoomIn();

  ZoomBubbleGtk::Close();
  DCHECK(!GetZoomBubble());
  EXPECT_FALSE(ZoomBubbleIsShowing());

  GtkWidget* window = GTK_WIDGET(browser()->window()->GetNativeWindow());
  GtkWidget* zoom_icon = ViewIDUtil::GetWidget(window, VIEW_ID_ZOOM_BUTTON);
  DCHECK(zoom_icon);

  TabContents* tab_contents = chrome::GetActiveTabContents(browser());
  DCHECK(tab_contents);

  // Force show a bubble.
  ZoomBubbleGtk::Show(zoom_icon, tab_contents, true);
  DCHECK(GetZoomBubble());
  EXPECT_TRUE(ZoomBubbleIsShowing());
}

IN_PROC_BROWSER_TEST_F(ZoomBubbleGtkTest, DefaultToZoomedInAndBack) {
  content::WebContents* contents = SetUpTest();

  ZoomIn();
  ExpectZoomedIn(contents);
  EXPECT_TRUE(ZoomBubbleIsShowing());
  ExpectLabelTextContainsZoom();

  ZoomOut();
  ExpectAtDefaultZoom(contents);
  EXPECT_FALSE(ZoomBubbleIsShowing());
}

IN_PROC_BROWSER_TEST_F(ZoomBubbleGtkTest, ZoomInTwiceAndReset) {
  content::WebContents* contents = SetUpTest();

  ZoomIn();
  int zoom_level = GetZoomPercent(contents);
  ZoomIn();
  EXPECT_GT(GetZoomPercent(contents), zoom_level);
  ExpectZoomedIn(contents);

  EXPECT_TRUE(ZoomBubbleIsShowing());
  ExpectLabelTextContainsZoom();

  ResetZoom();
  ExpectAtDefaultZoom(contents);
  EXPECT_FALSE(ZoomBubbleIsShowing());
}

IN_PROC_BROWSER_TEST_F(ZoomBubbleGtkTest, DefaultToZoomedOutAndBack) {
  content::WebContents* contents = SetUpTest();

  ZoomOut();
  ExpectZoomedOut(contents);
  EXPECT_TRUE(ZoomBubbleIsShowing());
  ExpectLabelTextContainsZoom();

  ZoomIn();
  ExpectAtDefaultZoom(contents);
  EXPECT_FALSE(ZoomBubbleIsShowing());
}

IN_PROC_BROWSER_TEST_F(ZoomBubbleGtkTest, ZoomOutTwiceAndReset) {
  content::WebContents* contents = SetUpTest();

  ZoomOut();
  int zoom_level = GetZoomPercent(contents);
  ZoomOut();
  EXPECT_LT(GetZoomPercent(contents), zoom_level);
  ExpectZoomedOut(contents);

  EXPECT_TRUE(ZoomBubbleIsShowing());
  ExpectLabelTextContainsZoom();

  ResetZoom();
  ExpectAtDefaultZoom(contents);
  EXPECT_FALSE(ZoomBubbleIsShowing());
}

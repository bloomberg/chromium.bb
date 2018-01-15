// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/common/page_zoom.h"

class LocationBarViewBrowserTest : public InProcessBrowserTest {
 public:
  LocationBarViewBrowserTest() = default;

  LocationBarView* GetLocationBarView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetLocationBarView();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LocationBarViewBrowserTest);
};

// Ensure the location bar decoration is added when zooming, and is removed when
// the bubble is closed, but only if zoom was reset.
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, LocationBarDecoration) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  ZoomView* zoom_view = GetLocationBarView()->zoom_view();

  ASSERT_TRUE(zoom_view);
  EXPECT_FALSE(zoom_view->visible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // Altering zoom should display a bubble. Note ZoomBubbleView closes
  // asynchronously, so precede checks with a run loop flush.
  zoom_controller->SetZoomLevel(content::ZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->visible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Close the bubble at other than 100% zoom. Icon should remain visible.
  ZoomBubbleView::CloseCurrentBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->visible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // Show the bubble again.
  zoom_controller->SetZoomLevel(content::ZoomFactorToZoomLevel(2.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->visible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Remains visible at 100% until the bubble is closed.
  zoom_controller->SetZoomLevel(content::ZoomFactorToZoomLevel(1.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->visible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Closing at 100% hides the icon.
  ZoomBubbleView::CloseCurrentBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(zoom_view->visible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/page_info/page_info_bubble_controller.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/material_design/material_design_controller.h"
#import "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/widget/widget.h"
#include "url/url_constants.h"

@interface PageInfoBubbleController (ExposedForTesting)
+ (PageInfoBubbleController*)getPageInfoBubbleForTest;
- (void)performLayout;
@end

namespace {

using PageInfoBubbleViewsMacTest = InProcessBrowserTest;

}  // namespace

// Test the Page Info bubble doesn't crash upon entering full screen mode while
// it is open. This may occur when the bubble is trying to reanchor itself.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewsMacTest, NoCrashOnFullScreenToggle) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  ShowPageInfoDialog(browser()->tab_strip_model()->GetWebContentsAt(0));
  ExclusiveAccessManager* access_manager =
      browser()->exclusive_access_manager();
  FullscreenController* fullscreen_controller =
      access_manager->fullscreen_controller();

  fullscreen_controller->ToggleBrowserFullscreenMode();
  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    views::BubbleDialogDelegateView* page_info =
        PageInfoBubbleView::GetPageInfoBubble();
    EXPECT_TRUE(page_info);
    views::Widget* page_info_bubble = page_info->GetWidget();
    EXPECT_TRUE(page_info_bubble);
    EXPECT_EQ(PageInfoBubbleView::BUBBLE_PAGE_INFO,
              PageInfoBubbleView::GetShownBubbleType());
    EXPECT_TRUE(page_info_bubble->IsVisible());
  } else {
    EXPECT_TRUE([PageInfoBubbleController getPageInfoBubbleForTest]);
    // In Cocoa, the crash occurs when the bubble tries to re-layout.
    [[PageInfoBubbleController getPageInfoBubbleForTest] performLayout];
  }

  // There should be no crash here from re-anchoring the Page Info bubble while
  // transitioning into full screen.
  fake_fullscreen.FinishTransition();
}

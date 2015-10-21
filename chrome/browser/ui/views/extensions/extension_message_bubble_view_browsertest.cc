// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_message_bubble_browsertest.h"
#include "chrome/browser/ui/views/extensions/extension_message_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

namespace {

// Returns the ToolbarView for the given |browser|.
ToolbarView* GetToolbarViewForBrowser(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)->toolbar();
}

// Checks that the |bubble| is using the |expected_reference_view|, and is in
// approximately the correct position.
void CheckBubbleAndReferenceView(views::BubbleDelegateView* bubble,
                                 views::View* expected_reference_view) {
  ASSERT_TRUE(bubble);
  ASSERT_TRUE(expected_reference_view);
  EXPECT_EQ(expected_reference_view, bubble->GetAnchorView());

  // Do a rough check that the bubble is in the right place.
  gfx::Rect bubble_bounds = bubble->GetBoundsInScreen();
  gfx::Rect reference_bounds = expected_reference_view->GetBoundsInScreen();
  // It should be below the reference view, but not too far below.
  EXPECT_GE(bubble_bounds.y(), reference_bounds.bottom());
  // "Too far below" is kind of ambiguous. The exact logic of where a bubble
  // is positioned with respect to its anchor view should be tested as part of
  // the bubble logic, but we still want to make sure we didn't accidentally
  // place it somewhere crazy (which can happen if we draw it, and then
  // animate or reposition the reference view).
  const int kFudgeFactor = 50;
  EXPECT_LE(bubble_bounds.y(), reference_bounds.bottom() + kFudgeFactor);
  // The bubble should intersect the reference view somewhere along the x-axis.
  EXPECT_FALSE(bubble_bounds.x() > reference_bounds.right());
  EXPECT_FALSE(reference_bounds.x() > bubble_bounds.right());

  // And, of course, the bubble should be visible...
  EXPECT_TRUE(bubble->visible());
  // ... as should its Widget.
  EXPECT_TRUE(bubble->GetWidget()->IsVisible());
}

}  // namespace

class ExtensionMessageBubbleViewBrowserTest
    : public ExtensionMessageBubbleBrowserTest {
 protected:
  ExtensionMessageBubbleViewBrowserTest() {
    extensions::ExtensionMessageBubbleView::
        set_bubble_appearance_wait_time_for_testing(0);
  }
  ~ExtensionMessageBubbleViewBrowserTest() override {}

 private:
  // ExtensionMessageBubbleBrowserTest:
  void CheckBubble(Browser* browser, AnchorPosition anchor) override;
  void CloseBubble(Browser* browser) override;
  void CheckBubbleIsNotPresent(Browser* browser) override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleViewBrowserTest);
};

void ExtensionMessageBubbleViewBrowserTest::CheckBubble(Browser* browser,
                                                        AnchorPosition anchor) {
  ToolbarView* toolbar_view = GetToolbarViewForBrowser(browser);
  BrowserActionsContainer* container = toolbar_view->browser_actions();
  views::BubbleDelegateView* bubble = container->active_bubble();
  views::View* anchor_view = nullptr;
  switch (anchor) {
    case ANCHOR_BROWSER_ACTION:
      anchor_view = container->GetToolbarActionViewAt(0);
      break;
    case ANCHOR_APP_MENU:
      anchor_view = toolbar_view->app_menu_button();
      break;
  }
  CheckBubbleAndReferenceView(bubble, anchor_view);
}

void ExtensionMessageBubbleViewBrowserTest::CloseBubble(Browser* browser) {
  BrowserActionsContainer* container =
      GetToolbarViewForBrowser(browser)->browser_actions();
  views::BubbleDelegateView* bubble = container->active_bubble();
  ASSERT_TRUE(bubble);
  bubble->GetWidget()->Close();
  EXPECT_EQ(nullptr, container->active_bubble());
}

void ExtensionMessageBubbleViewBrowserTest::CheckBubbleIsNotPresent(
    Browser* browser) {
  EXPECT_EQ(
      nullptr,
      GetToolbarViewForBrowser(browser)->browser_actions()->active_bubble());
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToExtensionAction) {
  TestBubbleAnchoredToExtensionAction();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToAppMenu) {
  TestBubbleAnchoredToAppMenu();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToAppMenuWithOtherAction) {
  TestBubbleAnchoredToAppMenuWithOtherAction();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       PRE_ExtensionBubbleShowsOnStartup) {
  PreBubbleShowsOnStartup();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleShowsOnStartup) {
  TestBubbleShowsOnStartup();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestUninstallDangerousExtension) {
  TestUninstallDangerousExtension();
}

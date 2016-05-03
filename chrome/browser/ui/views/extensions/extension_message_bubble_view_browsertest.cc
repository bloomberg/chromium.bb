// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_browsertest.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

namespace {

// Returns the ToolbarView for the given |browser|.
ToolbarView* GetToolbarViewForBrowser(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)->toolbar();
}

// Checks that the |bubble| is using the |expected_reference_view|, and is in
// approximately the correct position.
void CheckBubbleAndReferenceView(views::BubbleDialogDelegateView* bubble,
                                 views::View* expected_reference_view) {
  ASSERT_TRUE(bubble);
  ASSERT_TRUE(expected_reference_view);
  EXPECT_EQ(expected_reference_view, bubble->GetAnchorView());

  // Do a rough check that the bubble is in the right place.
  gfx::Rect bubble_bounds = bubble->GetWidget()->GetWindowBoundsInScreen();
  gfx::Rect reference_bounds = expected_reference_view->GetBoundsInScreen();
  // It should be below the reference view, but not too far below.
  EXPECT_GE(bubble_bounds.y(), reference_bounds.y());
  // The arrow should be poking into the anchor.
  EXPECT_LE(bubble_bounds.y(), reference_bounds.bottom());
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
  ExtensionMessageBubbleViewBrowserTest() {}
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
  views::BubbleDialogDelegateView* bubble = container->active_bubble();
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
  views::BubbleDialogDelegateView* bubble = container->active_bubble();
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

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestDevModeBubbleIsntShownTwice) {
  TestDevModeBubbleIsntShownTwice();
}

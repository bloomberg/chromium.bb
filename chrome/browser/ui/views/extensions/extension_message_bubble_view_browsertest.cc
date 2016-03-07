// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_browsertest.h"
#include "chrome/browser/ui/views/extensions/extension_message_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "ui/events/event_utils.h"

namespace extensions {

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
    ExtensionMessageBubbleView::set_bubble_appearance_wait_time_for_testing(0);
  }
  ~ExtensionMessageBubbleViewBrowserTest() override {}

  // Loads and returns an extension that overrides the new tab page.
  const Extension* LoadNewTabExtension();

  // Clicks on the |bubble|'s dismiss button.
  void ClickDismissButton(ExtensionMessageBubbleView* bubble) {
    ClickMessageBubbleButton(bubble, bubble->dismiss_button_);
  }

  // Clicks on the |bubble|'s action button.
  void ClickActionButton(ExtensionMessageBubbleView* bubble) {
    ClickMessageBubbleButton(bubble, bubble->action_button_);
  }

  ExtensionMessageBubbleView* active_message_bubble() {
    return ExtensionMessageBubbleView::active_bubble_for_testing_;
  }

 private:
  void ClickMessageBubbleButton(ExtensionMessageBubbleView* bubble,
                                views::Button* button);

  // ExtensionMessageBubbleBrowserTest:
  void CheckBubble(Browser* browser, AnchorPosition anchor) override;
  void CloseBubble(Browser* browser) override;
  void CheckBubbleIsNotPresent(Browser* browser) override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleViewBrowserTest);
};

const Extension* ExtensionMessageBubbleViewBrowserTest::LoadNewTabExtension() {
  base::FilePath crx_path = PackExtension(test_data_dir_.AppendASCII("api_test")
                                              .AppendASCII("override")
                                              .AppendASCII("newtab"));
  EXPECT_FALSE(crx_path.empty());
  const Extension* extension = InstallExtensionFromWebstore(crx_path, 1);
  return extension;
}

void ExtensionMessageBubbleViewBrowserTest::ClickMessageBubbleButton(
    ExtensionMessageBubbleView* bubble,
    views::Button* button) {
  gfx::Point p(button->x(), button->y());
  ui::MouseEvent event(ui::ET_MOUSE_RELEASED, p, p, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  bubble->ButtonPressed(button, event);
}

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

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestClickingDismissButton) {
  const Extension* extension = LoadNewTabExtension();
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab/"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  NtpOverriddenBubbleDelegate bubble_delegate(profile());
  EXPECT_FALSE(bubble_delegate.HasBubbleInfoBeenAcknowledged(extension->id()));

  // Check the bubble is showing.
  base::RunLoop().RunUntilIdle();
  ExtensionMessageBubbleView* bubble = active_message_bubble();
  views::View* anchor_view =
      GetToolbarViewForBrowser(browser())->app_menu_button();
  CheckBubbleAndReferenceView(bubble, anchor_view);

  // Click the dismiss button. The bubble should close, and the extension should
  // be acknowledged (and still installed).
  ClickDismissButton(bubble);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(active_message_bubble());
  EXPECT_TRUE(bubble_delegate.HasBubbleInfoBeenAcknowledged(extension->id()));
  EXPECT_TRUE(ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
      extension_id));
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestClickingActionButton) {
  const Extension* extension = LoadNewTabExtension();
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab/"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  NtpOverriddenBubbleDelegate bubble_delegate(profile());
  EXPECT_FALSE(bubble_delegate.HasBubbleInfoBeenAcknowledged(extension->id()));

  // Check the bubble is showing.
  base::RunLoop().RunUntilIdle();
  ExtensionMessageBubbleView* bubble = active_message_bubble();
  views::View* anchor_view =
      GetToolbarViewForBrowser(browser())->app_menu_button();
  CheckBubbleAndReferenceView(bubble, anchor_view);

  // Click the action button. The bubble should close, and the extension should
  // not be acknowledged. Instead, the extension should be disabled.
  ClickActionButton(bubble);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(active_message_bubble());
  EXPECT_FALSE(bubble_delegate.HasBubbleInfoBeenAcknowledged(extension->id()));
  EXPECT_FALSE(ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
      extension_id));
  EXPECT_TRUE(ExtensionRegistry::Get(profile())->disabled_extensions().GetByID(
      extension_id));
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestBubbleFocusLoss) {
  const Extension* extension = LoadNewTabExtension();
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab/"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  NtpOverriddenBubbleDelegate bubble_delegate(profile());
  EXPECT_FALSE(bubble_delegate.HasBubbleInfoBeenAcknowledged(extension->id()));

  // Check the bubble is showing.
  base::RunLoop().RunUntilIdle();
  ExtensionMessageBubbleView* bubble = active_message_bubble();
  views::View* anchor_view =
      GetToolbarViewForBrowser(browser())->app_menu_button();
  CheckBubbleAndReferenceView(bubble, anchor_view);

  // Deactivate the bubble's widget (e.g. due to focus loss). This causes the
  // bubble to close, but the extension should not be acknowledged or removed.
  bubble->GetWidget()->Deactivate();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(active_message_bubble());
  EXPECT_FALSE(bubble_delegate.HasBubbleInfoBeenAcknowledged(extension->id()));
  EXPECT_TRUE(ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
      extension_id));
}

}  // namespace extensions

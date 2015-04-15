// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_factory.h"
#include "chrome/browser/ui/toolbar/browser_actions_bar_browsertest.h"
#include "chrome/browser/ui/views/extensions/extension_message_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/wrench_toolbar_button.h"
#include "extensions/common/feature_switch.h"

namespace {

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

  // And, of course, the bubble should be visible.
  EXPECT_TRUE(bubble->visible());
}

}  // namespace

class ExtensionMessageBubbleViewBrowserTest
    : public BrowserActionsBarBrowserTest {
 protected:
  ExtensionMessageBubbleViewBrowserTest() {}
  ~ExtensionMessageBubbleViewBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  scoped_ptr<extensions::FeatureSwitch::ScopedOverride>
      dev_mode_bubble_override_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleViewBrowserTest);
};

void ExtensionMessageBubbleViewBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  BrowserActionsBarBrowserTest::SetUpCommandLine(command_line);
  // The dev mode warning bubble is an easy one to trigger, so we use that for
  // our testing purposes.
  dev_mode_bubble_override_.reset(
      new extensions::FeatureSwitch::ScopedOverride(
          extensions::FeatureSwitch::force_dev_mode_highlighting(),
          true));
  ExtensionMessageBubbleFactory::set_enabled_for_tests(true);
}

// Tests that an extension bubble will be anchored to the wrench menu when there
// aren't any extensions with actions.
// This also tests that the crashes in crbug.com/476426 are fixed.
IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToWrenchMenu) {
  scoped_refptr<const extensions::Extension> action_extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "action_extension",
          extensions::extension_action_test_util::BROWSER_ACTION,
          extensions::Manifest::UNPACKED);
  extension_service()->AddExtension(action_extension.get());

  Browser* second_browser = new Browser(
      Browser::CreateParams(profile(), browser()->host_desktop_type()));
  base::RunLoop().RunUntilIdle();

  BrowserActionsContainer* second_container =
      BrowserView::GetBrowserViewForBrowser(second_browser)->toolbar()->
          browser_actions();
  views::BubbleDelegateView* bubble = second_container->active_bubble();
  CheckBubbleAndReferenceView(bubble,
                              second_container->GetToolbarActionViewAt(0));

  bubble->GetWidget()->Close();
  EXPECT_EQ(nullptr, second_container->active_bubble());
}

// Tests that an extension bubble will be anchored to an extension action when
// there are extensions with actions.
IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToExtensionAction) {
  scoped_refptr<const extensions::Extension> no_action_extension =
      extensions::extension_action_test_util::CreateActionExtension(
          "action_extension",
          extensions::extension_action_test_util::NO_ACTION,
          extensions::Manifest::UNPACKED);
  extension_service()->AddExtension(no_action_extension.get());

  Browser* second_browser = new Browser(
      Browser::CreateParams(profile(), browser()->host_desktop_type()));
  ASSERT_TRUE(second_browser);
  base::RunLoop().RunUntilIdle();

  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(second_browser)->toolbar();
  BrowserActionsContainer* second_container = toolbar->browser_actions();
  views::BubbleDelegateView* bubble = second_container->active_bubble();
  CheckBubbleAndReferenceView(bubble, toolbar->app_menu());

  bubble->GetWidget()->Close();
  EXPECT_EQ(nullptr, second_container->active_bubble());
}

// Tests that the extension bubble will show on startup.
IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       PRE_ExtensionBubbleShowsOnStartup) {
  LoadExtension(test_data_dir_.AppendASCII("good_unpacked"));
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleShowsOnStartup) {
  base::RunLoop().RunUntilIdle();
  BrowserActionsContainer* container =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar()->
          browser_actions();
  views::BubbleDelegateView* bubble = container->active_bubble();
  CheckBubbleAndReferenceView(bubble, container->GetToolbarActionViewAt(0));

  bubble->GetWidget()->Close();
  EXPECT_EQ(nullptr, container->active_bubble());
}

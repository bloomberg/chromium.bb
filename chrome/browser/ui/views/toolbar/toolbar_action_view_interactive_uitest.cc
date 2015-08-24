// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/wrench_menu.h"
#include "chrome/browser/ui/views/toolbar/wrench_toolbar_button.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

// Tests clicking on an overflowed toolbar action. This is called when the
// wrench menu is open, and handles actually clicking on the action.
void TestOverflowedToolbarAction(Browser* browser,
                                 const base::Closure& quit_closure) {
  // A bunch of plumbing to safely get at the overflowed toolbar action.
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar();
  EXPECT_TRUE(toolbar->IsWrenchMenuShowing());
  WrenchMenu* wrench_menu = toolbar->wrench_menu_for_testing();
  ASSERT_TRUE(wrench_menu);
  ExtensionToolbarMenuView* menu_view =
      wrench_menu->extension_toolbar_for_testing();
  ASSERT_TRUE(menu_view);
  BrowserActionsContainer* overflow_container =
      menu_view->container_for_testing();
  ASSERT_TRUE(overflow_container);
  ToolbarActionView* action_view =
      overflow_container->GetToolbarActionViewAt(0);
  EXPECT_TRUE(action_view->visible());

  // Left click on the toolbar action to activate it.
  gfx::Point action_view_loc =
      test::GetCenterInScreenCoordinates(action_view);
  ui_controls::SendMouseMove(action_view_loc.x(), action_view_loc.y());
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP, quit_closure);
}

}  // namespace

class ToolbarActionViewInteractiveUITest : public ExtensionBrowserTest {
 protected:
  ToolbarActionViewInteractiveUITest();
  ~ToolbarActionViewInteractiveUITest() override;

  // ExtensionBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void TearDownOnMainThread() override;

 private:
  // Override the extensions-action-redesign switch.
  scoped_ptr<extensions::FeatureSwitch::ScopedOverride> feature_override_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionViewInteractiveUITest);
};

ToolbarActionViewInteractiveUITest::ToolbarActionViewInteractiveUITest() {}
ToolbarActionViewInteractiveUITest::~ToolbarActionViewInteractiveUITest() {}

void ToolbarActionViewInteractiveUITest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  // We do this before the rest of the setup because it can affect how the views
  // are constructed.
  feature_override_.reset(new extensions::FeatureSwitch::ScopedOverride(
      extensions::FeatureSwitch::extension_action_redesign(), true));
  ToolbarActionsBar::disable_animations_for_testing_ = true;
}

void ToolbarActionViewInteractiveUITest::TearDownOnMainThread() {
  ToolbarActionsBar::disable_animations_for_testing_ = false;
}

#if defined(USE_OZONE)
// ozone bringup - http://crbug.com/401304
#define MAYBE_TestClickingOnOverflowedAction DISABLED_TestClickingOnOverflowedAction
#else
#define MAYBE_TestClickingOnOverflowedAction TestClickingOnOverflowedAction
#endif
// Tests clicking on an overflowed extension action.
IN_PROC_BROWSER_TEST_F(ToolbarActionViewInteractiveUITest,
                       MAYBE_TestClickingOnOverflowedAction) {
  // Load an extension.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("ui").AppendASCII("browser_action_popup")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  // Reduce visible count to 0 so that the action is overflowed.
  ToolbarActionsModel::Get(profile())->SetVisibleIconCount(0);

  // When the extension is activated, it will send a message that its popup
  // opened. Listen for the message.
  ExtensionTestMessageListener listener("Popup opened", false);

  ToolbarView* toolbar_view =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();

  // Click on the wrench.
  gfx::Point wrench_button_loc =
      test::GetCenterInScreenCoordinates(toolbar_view->app_menu());
  base::RunLoop loop;
  ui_controls::SendMouseMove(wrench_button_loc.x(), wrench_button_loc.y());
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      base::Bind(&TestOverflowedToolbarAction, browser(), loop.QuitClosure()));
  loop.Run();
  // The wrench menu should no longer me showing.
  EXPECT_FALSE(toolbar_view->IsWrenchMenuShowing());

  // And the extension should have been activated.
  listener.WaitUntilSatisfied();
}

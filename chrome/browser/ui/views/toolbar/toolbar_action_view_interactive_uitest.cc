// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
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

// Tests clicking on an overflowed toolbar action. This is called when the app
// menu is open, and handles actually clicking on the action.
void TestOverflowedToolbarAction(Browser* browser,
                                 const base::Closure& quit_closure) {
  // A bunch of plumbing to safely get at the overflowed toolbar action.
  WrenchToolbarButton* app_menu_button =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar()
          ->app_menu_button();
  EXPECT_TRUE(app_menu_button->IsMenuShowing());
  WrenchMenu* app_menu = app_menu_button->app_menu_for_testing();
  ASSERT_TRUE(app_menu);
  ExtensionToolbarMenuView* menu_view =
      app_menu->extension_toolbar_for_testing();
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

  WrenchToolbarButton* app_menu_button =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->app_menu_button();

  // Click on the app button.
  gfx::Point app_button_loc =
      test::GetCenterInScreenCoordinates(app_menu_button);
  base::RunLoop loop;
  ui_controls::SendMouseMove(app_button_loc.x(), app_button_loc.y());
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      base::Bind(&TestOverflowedToolbarAction, browser(), loop.QuitClosure()));
  loop.Run();
  // The app menu should no longer be showing.
  EXPECT_FALSE(app_menu_button->IsMenuShowing());

  // And the extension should have been activated.
  listener.WaitUntilSatisfied();
}

// Tests that clicking on the toolbar action a second time when the action is
// already open results in closing the popup, and doesn't re-open it.
IN_PROC_BROWSER_TEST_F(ToolbarActionViewInteractiveUITest,
                       DoubleClickToolbarActionToClose) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("ui").AppendASCII("browser_action_popup")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  BrowserActionsContainer* browser_actions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->browser_actions();
  ToolbarActionsBar* toolbar_actions_bar =
      browser_actions_container->toolbar_actions_bar();
  ToolbarActionView* toolbar_action_view =
      browser_actions_container->GetToolbarActionViewAt(0);

  // When the extension is activated, it will send a message that its popup
  // opened. Listen for the message.
  ExtensionTestMessageListener listener("Popup opened", false);

  // Click on the action, and wait for the popup to fully load.
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(
      test::GetCenterInScreenCoordinates(toolbar_action_view)));

  EXPECT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP));
  listener.WaitUntilSatisfied();

  ExtensionActionViewController* view_controller =
      static_cast<ExtensionActionViewController*>(
          toolbar_action_view->view_controller());
  EXPECT_EQ(view_controller, toolbar_actions_bar->popup_owner());
  EXPECT_TRUE(view_controller->is_showing_popup());

  {
    // Click down on the action button; this should close the popup.
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());
    // For reasons unbeknownst to me, SendMouseEventsSync() with only a down
    // event will cause Windows to hang. Using SendMouseEvents() and running all
    // pending UI tasks seems to do the trick.
    EXPECT_TRUE(ui_controls::SendMouseEvents(ui_controls::LEFT,
                                             ui_controls::DOWN));
    base::RunLoop loop;
    ui_controls::RunClosureAfterAllPendingUIEvents(loop.QuitClosure());
    loop.Run();
    observer.Wait();  // Wait for the popup to fully close.
  }

  EXPECT_FALSE(view_controller->is_showing_popup());
  EXPECT_EQ(nullptr, toolbar_actions_bar->popup_owner());

  // Releasing the mouse shouldn't result in the popup being shown again.
  EXPECT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
  EXPECT_FALSE(view_controller->is_showing_popup());
  EXPECT_EQ(nullptr, toolbar_actions_bar->popup_owner());
}

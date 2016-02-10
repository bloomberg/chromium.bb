// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace {

AppMenuButton* GetAppButtonFromBrowser(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar()
      ->app_menu_button();
}

// Tests clicking on an overflowed toolbar action. This is called when the app
// menu is open, and handles actually clicking on the action.
// |button| specifies the mouse button to click with.
void TestOverflowedToolbarAction(Browser* browser,
                                 ui_controls::MouseButton button,
                                 const base::Closure& quit_closure) {
  // A bunch of plumbing to safely get at the overflowed toolbar action.
  AppMenuButton* app_menu_button = GetAppButtonFromBrowser(browser);
  EXPECT_TRUE(app_menu_button->IsMenuShowing());
  AppMenu* app_menu = app_menu_button->app_menu_for_testing();
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

  // Click on the toolbar action to activate it.
  gfx::Point action_view_loc =
      ui_test_utils::GetCenterInScreenCoordinates(action_view);
  ui_controls::SendMouseMove(action_view_loc.x(), action_view_loc.y());
  ui_controls::SendMouseEventsNotifyWhenDone(
      button, ui_controls::DOWN | ui_controls::UP, quit_closure);
}

void ActivateOverflowedActionWithKeyboard(Browser* browser,
                                          const base::Closure& closure) {
  AppMenuButton* app_menu_button = GetAppButtonFromBrowser(browser);
  EXPECT_TRUE(app_menu_button->IsMenuShowing());
  // We need to dispatch key events to the menu's native window, rather than the
  // browser's.
  gfx::NativeWindow native_window =
      views::MenuController::GetActiveInstance()->owner()->GetNativeWindow();

  // Send two key down events followed by the return key.
  // The two key down events target the toolbar action in the app menu.
  // TODO(devlin): Shouldn't this be one key down event?
  ui_controls::SendKeyPress(native_window,
                            ui::VKEY_DOWN,
                            false, false, false, false);
  ui_controls::SendKeyPress(native_window,
                            ui::VKEY_DOWN,
                            false, false, false, false);
  ui_controls::SendKeyPressNotifyWhenDone(native_window,
                            ui::VKEY_RETURN,
                            false, false, false, false, closure);
}

// Tests the context menu of an overflowed action.
void TestWhileContextMenuOpen(bool* did_test_while_menu_open,
                              Browser* browser,
                              ToolbarActionView* context_menu_action) {
  *did_test_while_menu_open = true;

  views::MenuItemView* menu_root = context_menu_action->menu_for_testing();
  ASSERT_TRUE(menu_root);
  ASSERT_TRUE(menu_root->GetSubmenu());
  EXPECT_TRUE(menu_root->GetSubmenu()->IsShowing());
  views::MenuItemView* first_menu_item =
      menu_root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_TRUE(first_menu_item);

  // Make sure we're showing the right context menu.
  EXPECT_EQ(base::UTF8ToUTF16("Browser Action Popup"),
            first_menu_item->title());
  EXPECT_TRUE(first_menu_item->enabled());

  // Get the overflow container.
  AppMenuButton* app_menu_button = GetAppButtonFromBrowser(browser);
  AppMenu* app_menu = app_menu_button->app_menu_for_testing();
  ASSERT_TRUE(app_menu);
  ExtensionToolbarMenuView* menu_view =
      app_menu->extension_toolbar_for_testing();
  ASSERT_TRUE(menu_view);
  BrowserActionsContainer* overflow_container =
      menu_view->container_for_testing();
  ASSERT_TRUE(overflow_container);

  // Get the first action on the second row of the overflow container.
  int second_row_index = overflow_container->toolbar_actions_bar()
                             ->platform_settings()
                             .icons_per_overflow_menu_row;
  ToolbarActionView* second_row_action =
      overflow_container->GetToolbarActionViewAt(second_row_index);

  EXPECT_TRUE(second_row_action->visible());
  EXPECT_TRUE(second_row_action->enabled());

  gfx::Point action_view_loc =
      ui_test_utils::GetCenterInScreenCoordinates(second_row_action);
  gfx::Point action_view_loc_in_menu_item_bounds = action_view_loc;
  views::View::ConvertPointFromScreen(first_menu_item,
                                      &action_view_loc_in_menu_item_bounds);
  // Regression test for crbug.com/538414: The first menu item is overlapping
  // the second row action button. With crbug.com/538414, the click would go to
  // the menu button, instead of the menu item.
  EXPECT_TRUE(
      first_menu_item->HitTestPoint(action_view_loc_in_menu_item_bounds));

  // Click on the first menu item (which shares bounds, but overlaps, the second
  // row action).
  ui_controls::SendMouseMove(action_view_loc.x(), action_view_loc.y());
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP, base::Closure());

  // Test resumes in the main test body.
}

// Posts a task to test the context menu.
void OnContextMenuWillShow(bool* did_test_while_menu_open,
                           Browser* browser,
                           ToolbarActionView* toolbar_action_view) {
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&TestWhileContextMenuOpen, did_test_while_menu_open,
                            browser, toolbar_action_view));
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

  AppMenuButton* app_menu_button = GetAppButtonFromBrowser(browser());

  // Click on the app button.
  gfx::Point app_button_loc =
      ui_test_utils::GetCenterInScreenCoordinates(app_menu_button);
  base::RunLoop loop;
  ui_controls::SendMouseMove(app_button_loc.x(), app_button_loc.y());
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
      base::Bind(&TestOverflowedToolbarAction, browser(), ui_controls::LEFT,
                 loop.QuitClosure()));
  loop.Run();
  // The app menu should no longer be showing.
  EXPECT_FALSE(app_menu_button->IsMenuShowing());

  // And the extension should have been activated.
  listener.WaitUntilSatisfied();
}

#if defined(USE_OZONE)
// ozone bringup - http://crbug.com/401304
#define MAYBE_TestContextMenuOnOverflowedAction \
  DISABLED_TestContextMenuOnOverflowedAction
#else
#define MAYBE_TestContextMenuOnOverflowedAction \
  TestContextMenuOnOverflowedAction
#endif
// Tests the context menus of overflowed extension actions.
IN_PROC_BROWSER_TEST_F(ToolbarActionViewInteractiveUITest,
                       MAYBE_TestContextMenuOnOverflowedAction) {
  views::MenuController::TurnOffMenuSelectionHoldForTest();

  // Load an extension that has a home page (important for the context menu's
  // first item being enabled).
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("ui").AppendASCII("browser_action_popup")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  // Aaaannnnd... Load a bunch of other extensions so that the overflow menu
  // is spread across multiple rows.
  for (int i = 0; i < 15; ++i) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::extension_action_test_util::CreateActionExtension(
            base::IntToString(i),
            extensions::extension_action_test_util::BROWSER_ACTION);
    extension_service()->AddExtension(extension.get());
  }

  ASSERT_EQ(16u, browser()
                     ->window()
                     ->GetToolbarActionsBar()
                     ->toolbar_actions_unordered()
                     .size());

  // Reduce visible count to 0 so that all actions are overflowed.
  ToolbarActionsModel::Get(profile())->SetVisibleIconCount(0);

  // Set a callback for the context menu showing.
  bool did_test_while_menu_open = false;
  base::Callback<void(ToolbarActionView*)> context_menu_callback(
      base::Bind(&OnContextMenuWillShow, &did_test_while_menu_open, browser()));
  ToolbarActionView::set_context_menu_callback_for_testing(
      &context_menu_callback);

  AppMenuButton* app_menu_button = GetAppButtonFromBrowser(browser());
  // Click on the app button, and then right-click on the first toolbar action.
  gfx::Point app_button_loc =
      ui_test_utils::GetCenterInScreenCoordinates(app_menu_button);
  base::RunLoop loop;
  ui_controls::SendMouseMove(app_button_loc.x(), app_button_loc.y());
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
      base::Bind(&TestOverflowedToolbarAction, browser(), ui_controls::RIGHT,
                 loop.QuitClosure()));
  loop.Run();

  // Test is continued first in TestOverflowedToolbarAction() to right click on
  // the action, followed by OnContextMenuWillShow() and
  // TestWhileContextMenuOpen().

  // Make sure we did all the expected tests.
  EXPECT_TRUE(did_test_while_menu_open);

  // We should have navigated to the extension's home page, which is google.com.
  EXPECT_EQ(
      GURL("https://www.google.com/"),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
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
     ui_test_utils::GetCenterInScreenCoordinates(toolbar_action_view)));

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

#if defined(USE_OZONE)
// ozone bringup - http://crbug.com/401304
#define MAYBE_ActivateOverflowedToolbarActionWithKeyboard \
  DISABLED_ActivateOverflowedToolbarActionWithKeyboard
#else
#define MAYBE_ActivateOverflowedToolbarActionWithKeyboard \
  ActivateOverflowedToolbarActionWithKeyboard
#endif
IN_PROC_BROWSER_TEST_F(ToolbarActionViewInteractiveUITest,
                       MAYBE_ActivateOverflowedToolbarActionWithKeyboard) {
  views::MenuController::TurnOffMenuSelectionHoldForTest();
  // Load an extension with an action.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("ui").AppendASCII("browser_action_popup")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  // Reduce visible count to 0 so that all actions are overflowed.
  ToolbarActionsModel::Get(profile())->SetVisibleIconCount(0);

  // Set up a listener for the extension being triggered.
  ExtensionTestMessageListener listener("Popup opened", false);

  // Open the app menu, navigate to the toolbar action, and activate it via the
  // keyboard.
  AppMenuButton* app_menu_button = GetAppButtonFromBrowser(browser());
  gfx::Point app_button_loc =
      ui_test_utils::GetCenterInScreenCoordinates(app_menu_button);
  base::RunLoop loop;
  ui_controls::SendMouseMove(app_button_loc.x(), app_button_loc.y());
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
      base::Bind(&ActivateOverflowedActionWithKeyboard,
                 browser(), loop.QuitClosure()));
  loop.Run();

  // The app menu should no longer be showing.
  EXPECT_FALSE(app_menu_button->IsMenuShowing());
  // And the extension should have been activated.
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

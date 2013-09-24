// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include "apps/native_app_window.h"
#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_util.h"
#include "ash/launcher/launcher_view.h"
#include "ash/shell.h"
#include "ash/test/launcher_test_api.h"
#include "ash/test/launcher_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

using apps::ShellWindow;
using extensions::Extension;
using content::WebContents;

namespace {

class TestEvent : public ui::Event {
 public:
  explicit TestEvent(ui::EventType type)
      : ui::Event(type, base::TimeDelta(), 0) {
  }
  virtual ~TestEvent() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestEvent);
};

class TestShellWindowRegistryObserver
    : public apps::ShellWindowRegistry::Observer {
 public:
  explicit TestShellWindowRegistryObserver(Profile* profile)
      : profile_(profile),
        icon_updates_(0) {
    apps::ShellWindowRegistry::Get(profile_)->AddObserver(this);
  }

  virtual ~TestShellWindowRegistryObserver() {
    apps::ShellWindowRegistry::Get(profile_)->RemoveObserver(this);
  }

  // Overridden from ShellWindowRegistry::Observer:
  virtual void OnShellWindowAdded(ShellWindow* shell_window) OVERRIDE {}

  virtual void OnShellWindowIconChanged(ShellWindow* shell_window) OVERRIDE {
    ++icon_updates_;
  }

  virtual void OnShellWindowRemoved(ShellWindow* shell_window) OVERRIDE {}

  int icon_updates() { return icon_updates_; }

 private:
  Profile* profile_;
  int icon_updates_;

  DISALLOW_COPY_AND_ASSIGN(TestShellWindowRegistryObserver);
};

}  // namespace

class LauncherPlatformAppBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  LauncherPlatformAppBrowserTest() : launcher_(NULL), controller_(NULL) {
  }

  virtual ~LauncherPlatformAppBrowserTest() {}

  virtual void RunTestOnMainThreadLoop() OVERRIDE {
    launcher_ = ash::Launcher::ForPrimaryDisplay();
    controller_ = ChromeLauncherController::instance();
    return extensions::PlatformAppBrowserTest::RunTestOnMainThreadLoop();
  }

  ash::LauncherModel* launcher_model() {
    return ash::test::ShellTestApi(ash::Shell::GetInstance()).launcher_model();
  }

  ash::LauncherID CreateAppShortcutLauncherItem(const std::string& name) {
    return controller_->CreateAppShortcutLauncherItem(
        name, controller_->model()->item_count());
  }

  const ash::LauncherItem& GetLastLauncherItem() {
    // Unless there are any panels, the item at index [count - 1] will be
    // the desired item.
    return launcher_model()->items()[launcher_model()->item_count() - 1];
  }

  const ash::LauncherItem& GetLastLauncherPanelItem() {
    // Panels show up on the right side of the launcher, so the desired item
    // will be the last one.
    return launcher_model()->items()[launcher_model()->item_count() - 1];
  }

  LauncherItemController* GetItemController(ash::LauncherID id) {
    return controller_->id_to_item_controller_map_[id];
  }

  // Returns the number of menu items, ignoring separators.
  int GetNumApplicationMenuItems(const ash::LauncherItem& item) {
    const int event_flags = 0;
    scoped_ptr<ash::LauncherMenuModel> menu(
        controller_->CreateApplicationMenu(item, event_flags));
    int num_items = 0;
    for (int i = 0; i < menu->GetItemCount(); ++i) {
      if (menu->GetTypeAt(i) != ui::MenuModel::TYPE_SEPARATOR)
        ++num_items;
    }
    return num_items;
  }

  // Activate the launcher item with the given |id|.
  void ActivateLauncherItem(int id) {
    launcher_->ActivateLauncherItem(id);
  }

  ash::Launcher* launcher_;
  ChromeLauncherController* controller_;

 private:

  DISALLOW_COPY_AND_ASSIGN(LauncherPlatformAppBrowserTest);
};

enum RipOffCommand {
  // Drag the item off the shelf and let the mouse go.
  RIP_OFF_ITEM,
  // Drag the item off the shelf, move the mouse back and then let go.
  RIP_OFF_ITEM_AND_RETURN,
  // Drag the item off the shelf and then issue a cancel command.
  RIP_OFF_ITEM_AND_CANCEL,
  // Drag the item off the shelf and do not release the mouse.
  RIP_OFF_ITEM_AND_DONT_RELEASE_MOUSE,
};

class LauncherAppBrowserTest : public ExtensionBrowserTest {
 protected:
  LauncherAppBrowserTest() : launcher_(NULL), model_(NULL), controller_(NULL) {
  }

  virtual ~LauncherAppBrowserTest() {}

  virtual void RunTestOnMainThreadLoop() OVERRIDE {
    launcher_ = ash::Launcher::ForPrimaryDisplay();
    model_ =
        ash::test::ShellTestApi(ash::Shell::GetInstance()).launcher_model();
    controller_ = ChromeLauncherController::instance();
    return ExtensionBrowserTest::RunTestOnMainThreadLoop();
  }

  size_t NumberOfDetectedLauncherBrowsers(bool show_all_tabs) {
    LauncherItemController* item_controller =
      controller_->GetBrowserShortcutLauncherItemController();
    int items = item_controller->GetApplicationList(
        show_all_tabs ? ui::EF_SHIFT_DOWN : 0).size();
    // If we have at least one item, we have also a title which we remove here.
    return items ? (items - 1) : 0;
  }

  const Extension* LoadAndLaunchExtension(
      const char* name,
      extension_misc::LaunchContainer container,
      WindowOpenDisposition disposition) {
    EXPECT_TRUE(LoadExtension(test_data_dir_.AppendASCII(name)));

    ExtensionService* service = extensions::ExtensionSystem::Get(
        profile())->extension_service();
    const Extension* extension =
        service->GetExtensionById(last_loaded_extension_id_, false);
    EXPECT_TRUE(extension);

    chrome::OpenApplication(chrome::AppLaunchParams(profile(),
                                                    extension,
                                                    container,
                                                    disposition));
    return extension;
  }

  ash::LauncherID CreateShortcut(const char* name) {
    ExtensionService* service = extensions::ExtensionSystem::Get(
        profile())->extension_service();
    LoadExtension(test_data_dir_.AppendASCII(name));

    // First get app_id.
    const Extension* extension =
        service->GetExtensionById(last_loaded_extension_id_, false);
    const std::string app_id = extension->id();

    // Then create a shortcut.
    int item_count = model_->item_count();
    ash::LauncherID shortcut_id = controller_->CreateAppShortcutLauncherItem(
        app_id,
        item_count);
    controller_->PersistPinnedState();
    EXPECT_EQ(++item_count, model_->item_count());
    const ash::LauncherItem& item = *model_->ItemByID(shortcut_id);
    EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
    return item.id;
  }

  void RemoveShortcut(ash::LauncherID id) {
    controller_->Unpin(id);
  }

  // Activate the launcher item with the given |id|.
  void ActivateLauncherItem(int id) {
    launcher_->ActivateLauncherItem(id);
  }

  ash::LauncherID PinFakeApp(const std::string& name) {
    return controller_->CreateAppShortcutLauncherItem(
        name, model_->item_count());
  }

  // Get the index of an item which has the given type.
  int GetIndexOfLauncherItemType(ash::LauncherItemType type) {
    for (int i = 0; i < model_->item_count(); i++) {
      if (model_->items()[i].type == type)
        return i;
    }
    return -1;
  }

  // Try to rip off |item_index|.
  void RipOffItemIndex(int index,
                       aura::test::EventGenerator* generator,
                       ash::test::LauncherViewTestAPI* test,
                       RipOffCommand command) {
    ash::internal::LauncherButton* button = test->GetButton(index);
    gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
    gfx::Point rip_off_point(0, 0);
    generator->MoveMouseTo(start_point.x(), start_point.y());
    base::MessageLoop::current()->RunUntilIdle();
    generator->PressLeftButton();
    base::MessageLoop::current()->RunUntilIdle();
    generator->MoveMouseTo(rip_off_point.x(), rip_off_point.y());
    base::MessageLoop::current()->RunUntilIdle();
    test->RunMessageLoopUntilAnimationsDone();
    if (command == RIP_OFF_ITEM_AND_RETURN) {
      generator->MoveMouseTo(start_point.x(), start_point.y());
      base::MessageLoop::current()->RunUntilIdle();
      test->RunMessageLoopUntilAnimationsDone();
    } else if (command == RIP_OFF_ITEM_AND_CANCEL) {
      // This triggers an internal cancel. Using VKEY_ESCAPE was too unreliable.
      button->OnMouseCaptureLost();
    }
    if (command != RIP_OFF_ITEM_AND_DONT_RELEASE_MOUSE) {
      generator->ReleaseLeftButton();
      test->RunMessageLoopUntilAnimationsDone();
    }
  }

  ash::Launcher* launcher_;
  ash::LauncherModel* model_;
  ChromeLauncherController* controller_;

 private:

  DISALLOW_COPY_AND_ASSIGN(LauncherAppBrowserTest);
};

class LauncherAppBrowserTestNoDefaultBrowser : public LauncherAppBrowserTest {
 protected:
  LauncherAppBrowserTestNoDefaultBrowser() {}
  virtual ~LauncherAppBrowserTestNoDefaultBrowser() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    LauncherAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(LauncherAppBrowserTestNoDefaultBrowser);
};

// Since the default for minimizing on click might change, I added both classes
// to either get the minimize on click or not.
class LauncherAppBrowserNoMinimizeOnClick
    : public LauncherPlatformAppBrowserTest {
 protected:
  LauncherAppBrowserNoMinimizeOnClick() {}
  virtual ~LauncherAppBrowserNoMinimizeOnClick() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    LauncherPlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kDisableMinimizeOnSecondLauncherItemClick);
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(LauncherAppBrowserNoMinimizeOnClick);
};

typedef LauncherPlatformAppBrowserTest LauncherAppBrowserMinimizeOnClick;

// Test that we can launch a platform app and get a running item.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, LaunchUnpinned) {
  int item_count = launcher_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  ShellWindow* window = CreateShellWindow(extension);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item = GetLastLauncherItem();
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);
  CloseShellWindow(window);
  --item_count;
  EXPECT_EQ(item_count, launcher_model()->item_count());
}

// Test that we can launch a platform app that already has a shortcut.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, LaunchPinned) {
  int item_count = launcher_model()->item_count();

  // First get app_id.
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  const std::string app_id = extension->id();

  // Then create a shortcut.
  ash::LauncherID shortcut_id = CreateAppShortcutLauncherItem(app_id);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  ash::LauncherItem item = *launcher_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);

  // Open a window. Confirm the item is now running.
  ShellWindow* window = CreateShellWindow(extension);
  ash::wm::ActivateWindow(window->GetNativeWindow());
  ASSERT_EQ(item_count, launcher_model()->item_count());
  item = *launcher_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);

  // Then close it, make sure there's still an item.
  CloseShellWindow(window);
  ASSERT_EQ(item_count, launcher_model()->item_count());
  item = *launcher_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, PinRunning) {
  // Run.
  int item_count = launcher_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  ShellWindow* window = CreateShellWindow(extension);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item1 = GetLastLauncherItem();
  ash::LauncherID id = item1.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);

  // Create a shortcut. The app item should be after it.
  ash::LauncherID foo_id = CreateAppShortcutLauncherItem("foo");
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  EXPECT_LT(launcher_model()->ItemIndexByID(foo_id),
            launcher_model()->ItemIndexByID(id));

  // Pin the app. The item should remain.
  controller_->Pin(id);
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item2 = *launcher_model()->ItemByID(id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item2.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item2.status);

  // New shortcuts should come after the item.
  ash::LauncherID bar_id = CreateAppShortcutLauncherItem("bar");
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  EXPECT_LT(launcher_model()->ItemIndexByID(id),
            launcher_model()->ItemIndexByID(bar_id));

  // Then close it, make sure the item remains.
  CloseShellWindow(window);
  ASSERT_EQ(item_count, launcher_model()->item_count());
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, UnpinRunning) {
  int item_count = launcher_model()->item_count();

  // First get app_id.
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  const std::string app_id = extension->id();

  // Then create a shortcut.
  ash::LauncherID shortcut_id = CreateAppShortcutLauncherItem(app_id);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  ash::LauncherItem item = *launcher_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);

  // Create a second shortcut. This will be needed to force the first one to
  // move once it gets unpinned.
  ash::LauncherID foo_id = CreateAppShortcutLauncherItem("foo");
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  EXPECT_LT(launcher_model()->ItemIndexByID(shortcut_id),
            launcher_model()->ItemIndexByID(foo_id));

  // Open a window. Confirm the item is now running.
  ShellWindow* window = CreateShellWindow(extension);
  ash::wm::ActivateWindow(window->GetNativeWindow());
  ASSERT_EQ(item_count, launcher_model()->item_count());
  item = *launcher_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);

  // Unpin the app. The item should remain.
  controller_->Unpin(shortcut_id);
  ASSERT_EQ(item_count, launcher_model()->item_count());
  item = *launcher_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);
  // The item should have moved after the other shortcuts.
  EXPECT_GT(launcher_model()->ItemIndexByID(shortcut_id),
            launcher_model()->ItemIndexByID(foo_id));

  // Then close it, make sure the item's gone.
  CloseShellWindow(window);
  --item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
}

// Test that we can launch a platform app with more than one window.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, MultipleWindows) {
  int item_count = launcher_model()->item_count();

  // First run app.
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  ShellWindow* window1 = CreateShellWindow(extension);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item1 = GetLastLauncherItem();
  ash::LauncherID item_id = item1.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  EXPECT_EQ(2, GetNumApplicationMenuItems(item1));  // Title + 1 window

  // Add second window.
  ShellWindow* window2 = CreateShellWindow(extension);
  // Confirm item stays.
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item2 = *launcher_model()->ItemByID(item_id);
  EXPECT_EQ(ash::STATUS_ACTIVE, item2.status);
  EXPECT_EQ(3, GetNumApplicationMenuItems(item2));  // Title + 2 windows

  // Close second window.
  CloseShellWindow(window2);
  // Confirm item stays.
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item3 = *launcher_model()->ItemByID(item_id);
  EXPECT_EQ(ash::STATUS_ACTIVE, item3.status);
  EXPECT_EQ(2, GetNumApplicationMenuItems(item3));  // Title + 1 window

  // Close first window.
  CloseShellWindow(window1);
  // Confirm item is removed.
  --item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, MultipleApps) {
  int item_count = launcher_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch");
  ShellWindow* window1 = CreateShellWindow(extension1);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item1 = GetLastLauncherItem();
  ash::LauncherID item_id1 = item1.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);

  // Then run second app.
  const Extension* extension2 = LoadAndLaunchPlatformApp("launch_2");
  ShellWindow* window2 = CreateShellWindow(extension2);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item2 = GetLastLauncherItem();
  ash::LauncherID item_id2 = item2.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item2.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item2.status);

  EXPECT_NE(item_id1, item_id2);
  EXPECT_EQ(ash::STATUS_RUNNING,
            launcher_model()->ItemByID(item_id1)->status);

  // Close second app.
  CloseShellWindow(window2);
  --item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  // First app should be active again.
  EXPECT_EQ(ash::STATUS_ACTIVE,
            launcher_model()->ItemByID(item_id1)->status);

  // Close first app.
  CloseShellWindow(window1);
  --item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
}

// Confirm that app windows can be reactivated by clicking their icons and that
// the correct activation order is maintained.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, WindowActivation) {
  int item_count = launcher_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch");
  ShellWindow* window1 = CreateShellWindow(extension1);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item1 = GetLastLauncherItem();
  ash::LauncherID item_id1 = item1.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);

  // Then run second app.
  const Extension* extension2 = LoadAndLaunchPlatformApp("launch_2");
  ShellWindow* window2 = CreateShellWindow(extension2);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item2 = GetLastLauncherItem();
  ash::LauncherID item_id2 = item2.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item2.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item2.status);

  EXPECT_NE(item_id1, item_id2);
  EXPECT_EQ(ash::STATUS_RUNNING,
            launcher_model()->ItemByID(item_id1)->status);

  // Activate first one.
  ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id1));
  EXPECT_EQ(ash::STATUS_ACTIVE, launcher_model()->ItemByID(item_id1)->status);
  EXPECT_EQ(ash::STATUS_RUNNING,
            launcher_model()->ItemByID(item_id2)->status);
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));

  // Activate second one.
  ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id2));
  EXPECT_EQ(ash::STATUS_RUNNING,
            launcher_model()->ItemByID(item_id1)->status);
  EXPECT_EQ(ash::STATUS_ACTIVE, launcher_model()->ItemByID(item_id2)->status);
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));

  // Add window for app1. This will activate it.
  ShellWindow* window1b = CreateShellWindow(extension1);
  ash::wm::ActivateWindow(window1b->GetNativeWindow());
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));

  // Activate launcher item for app1, this will activate the first app window.
  ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id1));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));
  ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id1));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));

  // Activate the second app again
  ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id2));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));

  // Activate the first app again
  ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id1));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));

  // Close second app.
  CloseShellWindow(window2);
  --item_count;
  EXPECT_EQ(item_count, launcher_model()->item_count());
  // First app should be active again.
  EXPECT_EQ(ash::STATUS_ACTIVE, launcher_model()->ItemByID(item_id1)->status);

  // Close first app.
  CloseShellWindow(window1b);
  CloseShellWindow(window1);
  --item_count;
  EXPECT_EQ(item_count, launcher_model()->item_count());
}

// Confirm that Click behavior for app windows is correnct.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserNoMinimizeOnClick,
                       AppClickBehavior) {
  // Launch a platform app and create a window for it.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch");
  ShellWindow* window1 = CreateShellWindow(extension1);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  // Confirm that a controller item was created and is the correct state.
  const ash::LauncherItem& item1 = GetLastLauncherItem();
  LauncherItemController* item1_controller = GetItemController(item1.id);
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  EXPECT_EQ(LauncherItemController::TYPE_APP, item1_controller->type());
  // Clicking the item should have no effect.
  TestEvent click_event(ui::ET_MOUSE_PRESSED);
  item1_controller->Clicked(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  // Minimize the window and confirm that the controller item is updated.
  window1->GetBaseWindow()->Minimize();
  EXPECT_FALSE(window1->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);
  // Clicking the item should activate the window.
  item1_controller->Clicked(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  // Maximizing a window should preserve state after minimize + click.
  window1->GetBaseWindow()->Maximize();
  window1->GetBaseWindow()->Minimize();
  item1_controller->Clicked(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1->GetBaseWindow()->IsMaximized());
}

// Confirm the minimizing click behavior for apps.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserMinimizeOnClick,
                       PackagedAppClickBehaviorInMinimizeMode) {
  // Launch one platform app and create a window for it.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch");
  ShellWindow* window1 = CreateShellWindow(extension1);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());

  // Confirm that a controller item was created and is the correct state.
  const ash::LauncherItem& item1 = GetLastLauncherItem();
  LauncherItemController* item1_controller = GetItemController(item1.id);
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  EXPECT_EQ(LauncherItemController::TYPE_APP, item1_controller->type());
  // Since it is already active, clicking it should minimize.
  TestEvent click_event(ui::ET_MOUSE_PRESSED);
  item1_controller->Clicked(click_event);
  EXPECT_FALSE(window1->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1->GetBaseWindow()->IsMinimized());
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);
  // Clicking the item again should activate the window again.
  item1_controller->Clicked(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  // Maximizing a window should preserve state after minimize + click.
  window1->GetBaseWindow()->Maximize();
  window1->GetBaseWindow()->Minimize();
  item1_controller->Clicked(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1->GetBaseWindow()->IsMaximized());
  window1->GetBaseWindow()->Restore();
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window1->GetBaseWindow()->IsMaximized());

  // Creating a second window of the same type should change the behavior so
  // that a click does not change the activation state.
  ShellWindow* window1a = CreateShellWindow(extension1);
  EXPECT_TRUE(window1a->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1a->GetBaseWindow()->IsActive());
  // The first click does nothing.
  item1_controller->Clicked(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1a->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window1a->GetBaseWindow()->IsActive());
  // The second neither.
  item1_controller->Clicked(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1a->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window1a->GetBaseWindow()->IsActive());
}

// Confirm that click behavior for app panels is correct.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, AppPanelClickBehavior) {
  // Enable experimental APIs to allow panel creation.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableExperimentalExtensionApis);
  // Launch a platform app and create a panel window for it.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch");
  ShellWindow::CreateParams params;
  params.window_type = ShellWindow::WINDOW_TYPE_PANEL;
  params.focused = false;
  ShellWindow* panel = CreateShellWindowFromParams(extension1, params);
  EXPECT_TRUE(panel->GetNativeWindow()->IsVisible());
  // Panels should not be active by default.
  EXPECT_FALSE(panel->GetBaseWindow()->IsActive());
  // Confirm that a controller item was created and is the correct state.
  const ash::LauncherItem& item1 = GetLastLauncherPanelItem();
  LauncherItemController* item1_controller = GetItemController(item1.id);
  EXPECT_EQ(ash::TYPE_APP_PANEL, item1.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);
  EXPECT_EQ(LauncherItemController::TYPE_APP_PANEL, item1_controller->type());
  // Click the item and confirm that the panel is activated.
  TestEvent click_event(ui::ET_MOUSE_PRESSED);
  item1_controller->Clicked(click_event);
  EXPECT_TRUE(panel->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  // Click the item again and confirm that the panel is minimized.
  item1_controller->Clicked(click_event);
  EXPECT_TRUE(panel->GetBaseWindow()->IsMinimized());
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);
  // Click the item again and confirm that the panel is activated.
  item1_controller->Clicked(click_event);
  EXPECT_TRUE(panel->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(panel->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, BrowserActivation) {
  int item_count = launcher_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch");
  CreateShellWindow(extension1);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item1 = GetLastLauncherItem();
  ash::LauncherID item_id1 = item1.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);

  ash::wm::ActivateWindow(browser()->window()->GetNativeWindow());
  EXPECT_EQ(ash::STATUS_RUNNING,
            launcher_model()->ItemByID(item_id1)->status);
}

// Test that opening an app sets the correct icon
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, SetIcon) {
  TestShellWindowRegistryObserver test_observer(browser()->profile());

  // Enable experimental APIs to allow panel creation.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableExperimentalExtensionApis);

  int base_launcher_item_count = launcher_model()->item_count();
  ExtensionTestMessageListener launched_listener("Launched", false);
  ExtensionTestMessageListener completed_listener("Completed", false);
  LoadAndLaunchPlatformApp("app_icon");
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  ASSERT_TRUE(completed_listener.WaitUntilSatisfied());

  // Now wait until the WebContent has decoded the icons and chrome has
  // processed it. This needs to be in a loop since the renderer runs in a
  // different process.
  while (test_observer.icon_updates() < 3) {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // This test creates one shell window and one panel window.
  int launcher_item_count = launcher_model()->item_count();
  ASSERT_EQ(base_launcher_item_count + 2, launcher_item_count);
  // The Panel will be the last item, the app second-to-last.
  const ash::LauncherItem& app_item =
      launcher_model()->items()[launcher_item_count - 2];
  const ash::LauncherItem& panel_item =
      launcher_model()->items()[launcher_item_count - 1];
  const LauncherItemController* app_item_controller =
      GetItemController(app_item.id);
  const LauncherItemController* panel_item_controller =
      GetItemController(panel_item.id);
  // Icons for Apps are set by the ShellWindowLauncherController, so
  // image_set_by_controller() should be set.
  EXPECT_TRUE(app_item_controller->image_set_by_controller());
  EXPECT_TRUE(panel_item_controller->image_set_by_controller());
  // Ensure icon heights are correct (see test.js in app_icon/ test directory)
  EXPECT_EQ(48, app_item.image.height());
  EXPECT_EQ(64, panel_item.image.height());
}

// Test that we can launch an app with a shortcut.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, LaunchPinned) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);
  ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);
  WebContents* tab = tab_strip->GetActiveWebContents();
  content::WindowedNotificationObserver close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<WebContents>(tab));
  browser()->tab_strip_model()->CloseSelectedTabs();
  close_observer.Wait();
  EXPECT_EQ(--tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);
}

// Launch the app first and then create the shortcut.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, LaunchUnpinned) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  LoadAndLaunchExtension("app1", extension_misc::LAUNCH_TAB,
                         NEW_FOREGROUND_TAB);
  EXPECT_EQ(++tab_count, tab_strip->count());
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);
  WebContents* tab = tab_strip->GetActiveWebContents();
  content::WindowedNotificationObserver close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<WebContents>(tab));
  browser()->tab_strip_model()->CloseSelectedTabs();
  close_observer.Wait();
  EXPECT_EQ(--tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);
}

// Launches an app in the background and then tries to open it. This is test for
// a crash we had.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, LaunchInBackground) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  LoadAndLaunchExtension("app1", extension_misc::LAUNCH_TAB,
                         NEW_BACKGROUND_TAB);
  EXPECT_EQ(++tab_count, tab_strip->count());
  ChromeLauncherController::instance()->LaunchApp(last_loaded_extension_id_, 0);
}

// Confirm that clicking a icon for an app running in one of 2 maxmized windows
// activates the right window.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, LaunchMaximized) {
  aura::Window* window1 = browser()->window()->GetNativeWindow();
  ash::wm::WindowState* window1_state = ash::wm::GetWindowState(window1);
  window1_state->Maximize();
  content::WindowedNotificationObserver open_observer(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::NotificationService::AllSources());
  chrome::NewEmptyWindow(browser()->profile(), chrome::HOST_DESKTOP_TYPE_ASH);
  open_observer.Wait();
  Browser* browser2 = content::Source<Browser>(open_observer.source()).ptr();
  aura::Window* window2 = browser2->window()->GetNativeWindow();
  TabStripModel* tab_strip = browser2->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::wm::GetWindowState(window2)->Maximize();

  ash::LauncherID shortcut_id = CreateShortcut("app1");
  ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);

  window1->Show();
  window1_state->Activate();
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut_id)).status);

  ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);
}

// Activating the same app multiple times should launch only a single copy.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, ActivateApp) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app1"));

  ChromeLauncherController::instance()->ActivateApp(extension->id(), 0);
  EXPECT_EQ(++tab_count, tab_strip->count());
  ChromeLauncherController::instance()->ActivateApp(extension->id(), 0);
  EXPECT_EQ(tab_count, tab_strip->count());
}

// Launching the same app multiple times should launch a copy for each call.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, LaunchApp) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app1"));

  ChromeLauncherController::instance()->LaunchApp(extension->id(), 0);
  EXPECT_EQ(++tab_count, tab_strip->count());
  ChromeLauncherController::instance()->LaunchApp(extension->id(), 0);
  EXPECT_EQ(++tab_count, tab_strip->count());
}

// Launch 2 apps and toggle which is active.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, MultipleApps) {
  int item_count = model_->item_count();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::LauncherID shortcut1 = CreateShortcut("app1");
  EXPECT_EQ(++item_count, model_->item_count());
  ash::LauncherID shortcut2 = CreateShortcut("app2");
  EXPECT_EQ(++item_count, model_->item_count());

  // Launch first app.
  ActivateLauncherItem(model_->ItemIndexByID(shortcut1));
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* tab1 = tab_strip->GetActiveWebContents();
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut1)).status);

  // Launch second app.
  ActivateLauncherItem(model_->ItemIndexByID(shortcut2));
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* tab2 = tab_strip->GetActiveWebContents();
  ASSERT_NE(tab1, tab2);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut2)).status);

  // Reactivate first app.
  ActivateLauncherItem(model_->ItemIndexByID(shortcut1));
  EXPECT_EQ(tab_count, tab_strip->count());
  EXPECT_EQ(tab_strip->GetActiveWebContents(), tab1);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut2)).status);

  // Open second tab for second app. This should activate it.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.example.com/path3/foo.html"),
      NEW_FOREGROUND_TAB,
      0);
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut2)).status);

  // Reactivate first app.
  ActivateLauncherItem(model_->ItemIndexByID(shortcut1));
  EXPECT_EQ(tab_count, tab_strip->count());
  EXPECT_EQ(tab_strip->GetActiveWebContents(), tab1);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut2)).status);

  // And second again. This time the second tab should become active.
  ActivateLauncherItem(model_->ItemIndexByID(shortcut2));
  EXPECT_EQ(tab_count, tab_strip->count());
  EXPECT_EQ(tab_strip->GetActiveWebContents(), tab2);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut2)).status);
}

// Confirm that a page can be navigated from and to while maintaining the
// correct running state.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, Navigation) {
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);
  ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);

  // Navigate away.
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://www.example.com/path0/bar.html"));
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);

  // Navigate back.
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://www.example.com/path1/foo.html"));
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);
}

IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, MultipleOwnedTabs) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  WebContents* first_tab = tab_strip->GetActiveWebContents();

  // Create new tab owned by app.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.example.com/path2/bar.html"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(++tab_count, tab_strip->count());
  // Confirm app is still active.
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);

  // Create new tab not owned by app.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.example.com/path3/foo.html"),
      NEW_FOREGROUND_TAB,
      0);
  EXPECT_EQ(++tab_count, tab_strip->count());
  // No longer active.
  EXPECT_EQ(ash::STATUS_RUNNING, model_->ItemByID(shortcut_id)->status);

  // Activating app makes first tab active again.
  ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(tab_strip->GetActiveWebContents(), first_tab);
}

IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, RefocusFilter) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  WebContents* first_tab = tab_strip->GetActiveWebContents();

  controller_->SetRefocusURLPatternForTest(
      shortcut_id, GURL("http://www.example.com/path1/*"));
  // Create new tab owned by app.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.example.com/path2/bar.html"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(++tab_count, tab_strip->count());
  // Confirm app is still active.
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);

  // Create new tab not owned by app.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.example.com/path3/foo.html"),
      NEW_FOREGROUND_TAB,
      0);
  EXPECT_EQ(++tab_count, tab_strip->count());
  // No longer active.
  EXPECT_EQ(ash::STATUS_RUNNING, model_->ItemByID(shortcut_id)->status);

  // Activating app makes first tab active again, because second tab isn't
  // in its refocus url path.
  ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(tab_strip->GetActiveWebContents(), first_tab);
}

IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, RefocusFilterLaunch) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  controller_->SetRefocusURLPatternForTest(
      shortcut_id, GURL("http://www.example.com/path1/*"));

  // Create new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.example2.com/path2/bar.html"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* first_tab = tab_strip->GetActiveWebContents();
  // Confirm app is not active.
  EXPECT_EQ(ash::STATUS_CLOSED, model_->ItemByID(shortcut_id)->status);

  // Activating app should launch new tab, because second tab isn't
  // in its refocus url path.
  ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* second_tab = tab_strip->GetActiveWebContents();
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  EXPECT_NE(first_tab, second_tab);
  EXPECT_EQ(tab_strip->GetActiveWebContents(), second_tab);
}

// Check the launcher activation state for applications and browser.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, ActivationStateCheck) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  // Get the browser item index
  int browser_index = ash::launcher::GetBrowserItemIndex(*controller_->model());
  EXPECT_TRUE(browser_index >= 0);

  // Even though we are just comming up, the browser should be active.
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->items()[browser_index].status);

  ash::LauncherID shortcut_id = CreateShortcut("app1");
  controller_->SetRefocusURLPatternForTest(
      shortcut_id, GURL("http://www.example.com/path1/*"));

  EXPECT_EQ(ash::STATUS_CLOSED, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->items()[browser_index].status);

  // Create new tab which would be the running app.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.example.com/path1/bar.html"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // There should never be two items active at the same time.
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[browser_index].status);

  tab_strip->ActivateTabAt(0, false);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->items()[browser_index].status);

  tab_strip->CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->items()[browser_index].status);

  ash::wm::DeactivateWindow(browser()->window()->GetNativeWindow());
  EXPECT_EQ(ash::STATUS_CLOSED, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[browser_index].status);
}

// Check that the launcher activation state for a V1 application stays closed
// even after an asynchronous browser event comes in after the tab got
// destroyed.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, AsyncActivationStateCheck) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ash::LauncherID shortcut_id = CreateShortcut("app1");
  controller_->SetRefocusURLPatternForTest(
      shortcut_id, GURL("http://www.example.com/path1/*"));

  EXPECT_EQ(ash::STATUS_CLOSED, model_->ItemByID(shortcut_id)->status);

  // Create new tab which would be the running app.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.example.com/path1/bar.html"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  // To address the issue of crbug.com/174050, the tab we are about to close
  // has to be active.
  tab_strip->ActivateTabAt(1, false);
  EXPECT_EQ(1, tab_strip->active_index());

  // Close the web contents.
  tab_strip->CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
  // The status should now be set to closed.
  EXPECT_EQ(ash::STATUS_CLOSED, model_->ItemByID(shortcut_id)->status);
}

// Checks that a windowed application does not add an item to the browser list.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTestNoDefaultBrowser,
    WindowedAppDoesNotAddToBrowser) {
  // Get the number of items in the browser menu.
  size_t items = NumberOfDetectedLauncherBrowsers(false);
  size_t running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(0u, items);
  EXPECT_EQ(0u, running_browser);

  LoadAndLaunchExtension("app1", extension_misc::LAUNCH_WINDOW, NEW_WINDOW);

  // No new browser should get detected, even though one more is running.
  EXPECT_EQ(0u, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(++running_browser, chrome::GetTotalBrowserCount());

  LoadAndLaunchExtension("app1", extension_misc::LAUNCH_TAB, NEW_WINDOW);

  // A new browser should get detected and one more should be running.
  EXPECT_EQ(NumberOfDetectedLauncherBrowsers(false), 1u);
  EXPECT_EQ(++running_browser, chrome::GetTotalBrowserCount());
}

// Checks the functionality to enumerate all browsers vs. all tabs.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTestNoDefaultBrowser,
    EnumerateALlBrowsersAndTabs) {
  // Create at least one browser.
  LoadAndLaunchExtension("app1", extension_misc::LAUNCH_TAB, NEW_WINDOW);
  size_t browsers = NumberOfDetectedLauncherBrowsers(false);
  size_t tabs = NumberOfDetectedLauncherBrowsers(true);

  // Create a second browser.
  LoadAndLaunchExtension("app1", extension_misc::LAUNCH_TAB, NEW_WINDOW);

  EXPECT_EQ(++browsers, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(++tabs, NumberOfDetectedLauncherBrowsers(true));

  // Create only a tab.
  LoadAndLaunchExtension("app1",
                         extension_misc::LAUNCH_TAB,
                         NEW_FOREGROUND_TAB);

  EXPECT_EQ(browsers, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(++tabs, NumberOfDetectedLauncherBrowsers(true));
}

// Check that the keyboard activation of a launcher item tabs properly through
// the items at hand.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, AltNumberTabsTabbing) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ash::LauncherID shortcut_id = CreateShortcut("app");
  controller_->SetRefocusURLPatternForTest(
      shortcut_id, GURL("http://www.example.com/path/*"));
  std::string url = "http://www.example.com/path/bla";

  int shortcut_index = model_->ItemIndexByID(shortcut_id);

  // Create an application handled browser tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  content::WebContents* content1 = tab_strip->GetActiveWebContents();

  // Create some other browser tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.test.com"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* content1a = tab_strip->GetActiveWebContents();

  // Make sure that the active tab is now our handled tab.
  EXPECT_NE(content1a, content1);

  // The active tab should still be the unnamed tab. Then we switch and reach
  // the first app and stay there.
  EXPECT_EQ(content1a, tab_strip->GetActiveWebContents());
  ActivateLauncherItem(shortcut_index);
  EXPECT_EQ(content1, tab_strip->GetActiveWebContents());
  ActivateLauncherItem(shortcut_index);
  EXPECT_EQ(content1, tab_strip->GetActiveWebContents());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* content2 = tab_strip->GetActiveWebContents();

  EXPECT_EQ(content2, browser()->tab_strip_model()->GetActiveWebContents());
  ActivateLauncherItem(shortcut_index);
  EXPECT_EQ(content1, browser()->tab_strip_model()->GetActiveWebContents());
  ActivateLauncherItem(shortcut_index);
  EXPECT_EQ(content2, browser()->tab_strip_model()->GetActiveWebContents());
}

// Check that the keyboard activation of a launcher item tabs properly through
// the items at hand.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest,
                       AltNumberAppsTabbing) {
  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch");
  ui::BaseWindow* window1 = CreateShellWindow(extension1)->GetBaseWindow();
  const ash::LauncherItem& item1 = GetLastLauncherItem();
  ash::LauncherID app_id = item1.id;
  int app_index = launcher_model()->ItemIndexByID(app_id);

  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);

  const Extension* extension2 = LoadAndLaunchPlatformApp("launch_2");
  ui::BaseWindow* window2 = CreateShellWindow(extension2)->GetBaseWindow();

  // By now the browser should be active. Issue Alt keystrokes several times to
  // see that we stay on that application.
  EXPECT_TRUE(window2->IsActive());
  ActivateLauncherItem(app_index);
  EXPECT_TRUE(window1->IsActive());
  ActivateLauncherItem(app_index);
  EXPECT_TRUE(window1->IsActive());

  ui::BaseWindow* window1a = CreateShellWindow(extension1)->GetBaseWindow();

  EXPECT_TRUE(window1a->IsActive());
  EXPECT_FALSE(window1->IsActive());
  ActivateLauncherItem(app_index);
  EXPECT_TRUE(window1->IsActive());
  ActivateLauncherItem(app_index);
  EXPECT_TRUE(window1a->IsActive());
}

// Test that we can launch a platform app panel and get a running item.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, LaunchPanelWindow) {
  int item_count = launcher_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  ShellWindow::CreateParams params;
  params.window_type = ShellWindow::WINDOW_TYPE_PANEL;
  params.focused = false;
  ShellWindow* window = CreateShellWindowFromParams(extension, params);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item = GetLastLauncherPanelItem();
  EXPECT_EQ(ash::TYPE_APP_PANEL, item.type);
  // Opening a panel does not activate it.
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);
  CloseShellWindow(window);
  --item_count;
  EXPECT_EQ(item_count, launcher_model()->item_count());
}

// Test attention states of windows.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, WindowAttentionStatus) {
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  ShellWindow::CreateParams params;
  params.window_type = ShellWindow::WINDOW_TYPE_PANEL;
  params.focused = false;
  ShellWindow* panel = CreateShellWindowFromParams(extension, params);
  EXPECT_TRUE(panel->GetNativeWindow()->IsVisible());
  // Panels should not be active by default.
  EXPECT_FALSE(panel->GetBaseWindow()->IsActive());
  // Confirm that a controller item was created and is the correct state.
  const ash::LauncherItem& item = GetLastLauncherPanelItem();
  LauncherItemController* item_controller = GetItemController(item.id);
  EXPECT_EQ(ash::TYPE_APP_PANEL, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);
  EXPECT_EQ(LauncherItemController::TYPE_APP_PANEL, item_controller->type());

  // App windows should go to attention state.
  panel->GetNativeWindow()->SetProperty(aura::client::kDrawAttentionKey, true);
  EXPECT_EQ(ash::STATUS_ATTENTION, item.status);

  // Click the item and confirm that the panel is activated.
  TestEvent click_event(ui::ET_MOUSE_PRESSED);
  item_controller->Clicked(click_event);
  EXPECT_TRUE(panel->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);

  // Active windows don't show attention.
  panel->GetNativeWindow()->SetProperty(aura::client::kDrawAttentionKey, true);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);
}

// Checks that the browser Alt "tabbing" is properly done.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTestNoDefaultBrowser,
    AltNumberBrowserTabbing) {
  // Get the number of items in the browser menu.
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  // The first activation should create a browser at index 1 (App List @ 0).
  launcher_->ActivateLauncherItem(1);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  // A second activation should not create a new instance.
  launcher_->ActivateLauncherItem(1);
  Browser* browser1 = chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
  EXPECT_TRUE(browser1);
  aura::Window* window1 = browser1->window()->GetNativeWindow();
  Browser* browser2 = CreateBrowser(profile());
  aura::Window* window2 = browser2->window()->GetNativeWindow();

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_NE(window1, window2);
  EXPECT_EQ(window2, ash::wm::GetActiveWindow());

  // Activate multiple times the switcher to see that the windows get activated.
  launcher_->ActivateLauncherItem(1);
  EXPECT_EQ(window1, ash::wm::GetActiveWindow());
  launcher_->ActivateLauncherItem(1);
  EXPECT_EQ(window2, ash::wm::GetActiveWindow());

  // Create a third browser - make sure that we do not toggle simply between
  // two windows.
  Browser* browser3 = CreateBrowser(profile());
  aura::Window* window3 = browser3->window()->GetNativeWindow();

  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_NE(window1, window3);
  EXPECT_NE(window2, window3);
  EXPECT_EQ(window3, ash::wm::GetActiveWindow());

  launcher_->ActivateLauncherItem(1);
  EXPECT_EQ(window1, ash::wm::GetActiveWindow());
  launcher_->ActivateLauncherItem(1);
  EXPECT_EQ(window2, ash::wm::GetActiveWindow());
  launcher_->ActivateLauncherItem(1);
  EXPECT_EQ(window3, ash::wm::GetActiveWindow());
  launcher_->ActivateLauncherItem(1);
  EXPECT_EQ(window1, ash::wm::GetActiveWindow());

  // Create anther app and make sure that none of our browsers is active.
  LoadAndLaunchExtension("app1", extension_misc::LAUNCH_TAB, NEW_WINDOW);
  EXPECT_NE(window1, ash::wm::GetActiveWindow());
  EXPECT_NE(window2, ash::wm::GetActiveWindow());

  // After activation our browser should be active again.
  launcher_->ActivateLauncherItem(1);
  EXPECT_EQ(window1, ash::wm::GetActiveWindow());
}

// Checks that after a session restore, we do not start applications on an
// activation.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, ActivateAfterSessionRestore) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Create a known application.
  ash::LauncherID shortcut_id = CreateShortcut("app1");

  // Create a new browser - without activating it - and load an "app" into it.
  Browser::CreateParams params =
      Browser::CreateParams(profile(), chrome::GetActiveDesktop());
  params.initial_show_state = ui::SHOW_STATE_INACTIVE;
  Browser* browser2 = new Browser(params);
  controller_->SetRefocusURLPatternForTest(
      shortcut_id, GURL("http://www.example.com/path/*"));
  std::string url = "http://www.example.com/path/bla";
  ui_test_utils::NavigateToURLWithDisposition(
      browser2,
      GURL(url),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Remember the number of tabs for each browser.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count1 = tab_strip->count();
  TabStripModel* tab_strip2 = browser2->tab_strip_model();
  int tab_count2 = tab_strip2->count();

  // Check that we have two browsers and the inactive browser remained inactive.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow()),
            browser());
  // Check that the LRU browser list does only contain the original browser.
  BrowserList* ash_browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  BrowserList::const_reverse_iterator it =
      ash_browser_list->begin_last_active();
  EXPECT_EQ(*it, browser());
  ++it;
  EXPECT_EQ(it, ash_browser_list->end_last_active());

  // Now request to either activate an existing app or create a new one.
  controller_->ItemSelected(*model_->ItemByID(shortcut_id),
                           ui::KeyEvent(ui::ET_KEY_RELEASED,
                                        ui::VKEY_RETURN,
                                        0,
                                        false));

  // Check that we have set focus on the existing application and nothing new
  // was created.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(tab_count1, tab_strip->count());
  EXPECT_EQ(tab_count2, tab_strip2->count());
  EXPECT_EQ(chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow()),
            browser2);
}

// Do various drag and drop interaction tests between the application list and
// the launcher.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, DragAndDrop) {
  // Get a number of interfaces we need.
  aura::test::EventGenerator generator(
      ash::Shell::GetPrimaryRootWindow(), gfx::Point());
  ash::test::LauncherViewTestAPI test(
      ash::test::LauncherTestAPI(launcher_).launcher_view());
  AppListService* service = AppListService::Get();

  // There should be two items in our launcher by this time.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(service->IsAppListVisible());

  // Open the app list menu and check that the drag and drop host was set.
  gfx::Rect app_list_bounds =
      test.launcher_view()->GetAppListButtonView()->GetBoundsInScreen();
  generator.MoveMouseTo(app_list_bounds.CenterPoint().x(),
                        app_list_bounds.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  generator.ClickLeftButton();

  EXPECT_TRUE(service->IsAppListVisible());
  app_list::AppsGridView* grid_view =
      app_list::AppsGridView::GetLastGridViewForTest();
  ASSERT_TRUE(grid_view);
  ASSERT_TRUE(grid_view->has_drag_and_drop_host_for_test());

  // There should be 2 items in our application list.
  const views::ViewModel* vm_grid = grid_view->view_model_for_test();
  EXPECT_EQ(2, vm_grid->view_size());

  // Test #1: Drag an app list which does not exist yet item into the
  // launcher. Keeping it dragged, see that a new item gets created. Continuing
  // to drag it out should remove it again.

  // Get over item #1 of the application list and press the mouse button.
  views::View* item1 = vm_grid->view_at(1);
  gfx::Rect bounds_grid_1 = item1->GetBoundsInScreen();
  generator.MoveMouseTo(bounds_grid_1.CenterPoint().x(),
                        bounds_grid_1.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  generator.PressLeftButton();

  EXPECT_FALSE(grid_view->forward_events_to_drag_and_drop_host_for_test());

  // Drag the item into the launcher and check that a new item gets created.
  const views::ViewModel* vm_launcher =
      test.launcher_view()->view_model_for_test();
  views::View* launcher1 = vm_launcher->view_at(1);
  gfx::Rect bounds_launcher_1 = launcher1->GetBoundsInScreen();
  generator.MoveMouseTo(bounds_launcher_1.CenterPoint().x(),
                        bounds_launcher_1.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();

  // Check that a new item got created.
  EXPECT_EQ(3, model_->item_count());
  EXPECT_TRUE(grid_view->forward_events_to_drag_and_drop_host_for_test());

  // Move it where the item originally was and check that it disappears again.
  generator.MoveMouseTo(bounds_grid_1.CenterPoint().x(),
                        bounds_grid_1.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(grid_view->forward_events_to_drag_and_drop_host_for_test());

  // Dropping it should keep the launcher as it originally was.
  generator.ReleaseLeftButton();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, model_->item_count());
  // There are a few animations which need finishing before we can continue.
  test.RunMessageLoopUntilAnimationsDone();
  // Move the mouse outside of the launcher.
  generator.MoveMouseTo(0, 0);

  // Test #2: Check that the unknown item dropped into the launcher will
  // create a new item.
  generator.MoveMouseTo(bounds_grid_1.CenterPoint().x(),
                        bounds_grid_1.CenterPoint().y());
  generator.PressLeftButton();
  generator.MoveMouseTo(bounds_launcher_1.CenterPoint().x(),
                        bounds_launcher_1.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(3, model_->item_count());
  EXPECT_TRUE(grid_view->forward_events_to_drag_and_drop_host_for_test());
  generator.ReleaseLeftButton();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(grid_view->forward_events_to_drag_and_drop_host_for_test());
  EXPECT_EQ(3, model_->item_count());  // It should be still there.
  test.RunMessageLoopUntilAnimationsDone();

  // Test #3: Check that the now known item dropped into the launcher will
  // not create a new item.
  generator.MoveMouseTo(bounds_grid_1.CenterPoint().x(),
                        bounds_grid_1.CenterPoint().y());
  generator.PressLeftButton();
  generator.MoveMouseTo(bounds_launcher_1.CenterPoint().x(),
                        bounds_launcher_1.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(3, model_->item_count());  // No new item got added.
  EXPECT_TRUE(grid_view->forward_events_to_drag_and_drop_host_for_test());
  generator.ReleaseLeftButton();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(grid_view->forward_events_to_drag_and_drop_host_for_test());
  EXPECT_EQ(3, model_->item_count());  // And it remains that way.

  // Test #4: Check that by pressing ESC the operation gets cancelled.
  generator.MoveMouseTo(bounds_grid_1.CenterPoint().x(),
                        bounds_grid_1.CenterPoint().y());
  generator.PressLeftButton();
  generator.MoveMouseTo(bounds_launcher_1.CenterPoint().x(),
                        bounds_launcher_1.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  // Issue an ESC and see that the operation gets cancelled.
  generator.PressKey(ui::VKEY_ESCAPE, 0);
  generator.ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_FALSE(grid_view->dragging());
  EXPECT_FALSE(grid_view->has_dragged_view());
  generator.ReleaseLeftButton();
  }

// Do tests for removal of items from the shelf by dragging.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, DragOffShelf) {
  aura::test::EventGenerator generator(
      ash::Shell::GetPrimaryRootWindow(), gfx::Point());
  ash::test::LauncherViewTestAPI test(
      ash::test::LauncherTestAPI(launcher_).launcher_view());

  // Create a known application and check that we have 3 items in the launcher.
  CreateShortcut("app1");
  EXPECT_EQ(3, model_->item_count());

  // Test #1: Ripping out the browser item should not change anything.
  int browser_index = GetIndexOfLauncherItemType(ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_LE(0, browser_index);
  RipOffItemIndex(browser_index, &generator, &test, RIP_OFF_ITEM);
  // => It should not have been removed and the location should be unchanged.
  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(browser_index,
            GetIndexOfLauncherItemType(ash::TYPE_BROWSER_SHORTCUT));
  // Make sure that the hide state has been unset after the snap back animation
  // finished.
  ash::internal::LauncherButton* button = test.GetButton(browser_index);
  EXPECT_FALSE(button->state() & ash::internal::LauncherButton::STATE_HIDDEN);

  // Test #2: Ripping out the application and canceling the operation should
  // not change anything.
  int app_index = GetIndexOfLauncherItemType(ash::TYPE_APP_SHORTCUT);
  EXPECT_LE(0, app_index);
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM_AND_CANCEL);
  // => It should not have been removed and the location should be unchanged.
  ASSERT_EQ(3, model_->item_count());
  EXPECT_EQ(app_index, GetIndexOfLauncherItemType(ash::TYPE_APP_SHORTCUT));

  // Test #3: Ripping out the application and moving it back in should not
  // change anything.
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM_AND_RETURN);
  // => It should not have been removed and the location should be unchanged.
  ASSERT_EQ(3, model_->item_count());
  // Through the operation the index might have changed.
  app_index = GetIndexOfLauncherItemType(ash::TYPE_APP_SHORTCUT);

  // Test #4: Ripping out the application should remove the item.
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM);
  // => It should not have been removed and the location should be unchanged.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(-1, GetIndexOfLauncherItemType(ash::TYPE_APP_SHORTCUT));

  // Test #5: Uninstalling an application while it is being ripped off should
  // not crash.
  ash::LauncherID app_id = CreateShortcut("app2");
  int app2_index = GetIndexOfLauncherItemType(ash::TYPE_APP_SHORTCUT);
  EXPECT_EQ(3, model_->item_count());  // And it remains that way.
  RipOffItemIndex(app2_index,
                  &generator,
                  &test,
                  RIP_OFF_ITEM_AND_DONT_RELEASE_MOUSE);
  RemoveShortcut(app_id);
  EXPECT_EQ(2, model_->item_count());  // The item should now be gone.
  generator.ReleaseLeftButton();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, model_->item_count());  // And it remains that way.
  EXPECT_EQ(-1, GetIndexOfLauncherItemType(ash::TYPE_APP_SHORTCUT));
}

// Check that clicking on an app launcher item launches a new browser.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, ClickItem) {
  // Get a number of interfaces we need.
  aura::test::EventGenerator generator(
      ash::Shell::GetPrimaryRootWindow(), gfx::Point());
  ash::test::LauncherViewTestAPI test(
      ash::test::LauncherTestAPI(launcher_).launcher_view());
  AppListService* service = AppListService::Get();
  // There should be two items in our launcher by this time.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(service->IsAppListVisible());

  // Open the app list menu and check that the drag and drop host was set.
  gfx::Rect app_list_bounds =
      test.launcher_view()->GetAppListButtonView()->GetBoundsInScreen();
  generator.MoveMouseTo(app_list_bounds.CenterPoint().x(),
                        app_list_bounds.CenterPoint().y());
  generator.ClickLeftButton();
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(service->IsAppListVisible());
  app_list::AppsGridView* grid_view =
      app_list::AppsGridView::GetLastGridViewForTest();
  ASSERT_TRUE(grid_view);
  const views::ViewModel* vm_grid = grid_view->view_model_for_test();
  EXPECT_EQ(2, vm_grid->view_size());
  gfx::Rect bounds_grid_1 = vm_grid->view_at(1)->GetBoundsInScreen();
  // Test now that a click does create a new application tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  generator.MoveMouseTo(bounds_grid_1.CenterPoint().x(),
                        bounds_grid_1.CenterPoint().y());
  generator.ClickLeftButton();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(tab_count + 1, tab_strip->count());
}

// Check LauncherItemController of Browser Shortcut functionality.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTestNoDefaultBrowser,
    BrowserShortcutLauncherItemController) {
  LauncherItemController* item_controller =
      controller_->GetBrowserShortcutLauncherItemController();

  // Get the number of browsers.
  size_t running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(0u, running_browser);
  EXPECT_FALSE(item_controller->IsOpen());

  // Activate. This creates new browser
  item_controller->Activate();
  // New Window is created.
  running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(1u, running_browser);
  EXPECT_TRUE(item_controller->IsOpen());

  // Minimize Window.
  ash::wm::WindowState* window_state = ash::wm::GetActiveWindowState();
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  // Activate again. This doesn't create new browser.
  // It activates window.
  item_controller->Activate();
  running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(1u, running_browser);
  EXPECT_TRUE(item_controller->IsOpen());
  EXPECT_FALSE(window_state->IsMinimized());
}

// Check that GetIDByWindow() returns |LauncherID| of the active tab.
IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, MatchingLauncherIDandActiveTab) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(2, model_->item_count());

  aura::Window* window = browser()->window()->GetNativeWindow();

  int browser_index = ash::launcher::GetBrowserItemIndex(*model_);
  ash::LauncherID browser_id = model_->items()[browser_index].id;
  EXPECT_EQ(browser_id, controller_->GetIDByWindow(window));

  ash::LauncherID app_id = CreateShortcut("app1");
  EXPECT_EQ(3, model_->item_count());

  // Creates a new tab for "app1" and checks that GetIDByWindow() returns
  // |LauncherID| of "app1".
  ActivateLauncherItem(model_->ItemIndexByID(app_id));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(app_id, controller_->GetIDByWindow(window));

  // Makes tab at index 0(NTP) as an active tab and checks that GetIDByWindow()
  // returns |LauncherID| of browser shortcut.
  browser()->tab_strip_model()->ActivateTabAt(0, false);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(browser_id, controller_->GetIDByWindow(window));
}

IN_PROC_BROWSER_TEST_F(LauncherAppBrowserTest, OverflowBubble) {
  // Make sure to have a browser window
  chrome::NewTab(browser());

  // No overflow yet.
  EXPECT_FALSE(launcher_->IsShowingOverflowBubble());

  ash::test::LauncherViewTestAPI test(
      ash::test::LauncherTestAPI(launcher_).launcher_view());

  int items_added = 0;
  while (!test.IsOverflowButtonVisible()) {
    std::string fake_app_id = base::StringPrintf("fake_app_%d", items_added);
    PinFakeApp(fake_app_id);

    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Now show overflow bubble.
  test.ShowOverflowBubble();
  EXPECT_TRUE(launcher_->IsShowingOverflowBubble());

  // Unpin first pinned app and there should be no crash.
  controller_->UnpinAppWithID(std::string("fake_app_0"));

  test.RunMessageLoopUntilAnimationsDone();
  EXPECT_FALSE(launcher_->IsShowingOverflowBubble());
}

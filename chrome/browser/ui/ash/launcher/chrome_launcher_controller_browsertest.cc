// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/test/app_list_controller_test_api.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/settings_window_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

using apps::AppWindow;
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

class TestAppWindowRegistryObserver : public apps::AppWindowRegistry::Observer {
 public:
  explicit TestAppWindowRegistryObserver(Profile* profile)
      : profile_(profile), icon_updates_(0) {
    apps::AppWindowRegistry::Get(profile_)->AddObserver(this);
  }

  virtual ~TestAppWindowRegistryObserver() {
    apps::AppWindowRegistry::Get(profile_)->RemoveObserver(this);
  }

  // Overridden from AppWindowRegistry::Observer:
  virtual void OnAppWindowIconChanged(AppWindow* app_window) OVERRIDE {
    ++icon_updates_;
  }

  int icon_updates() { return icon_updates_; }

 private:
  Profile* profile_;
  int icon_updates_;

  DISALLOW_COPY_AND_ASSIGN(TestAppWindowRegistryObserver);
};

}  // namespace

class LauncherPlatformAppBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  LauncherPlatformAppBrowserTest() : shelf_(NULL), controller_(NULL) {
  }

  virtual ~LauncherPlatformAppBrowserTest() {}

  virtual void RunTestOnMainThreadLoop() OVERRIDE {
    shelf_ = ash::Shelf::ForPrimaryDisplay();
    controller_ = ChromeLauncherController::instance();
    return extensions::PlatformAppBrowserTest::RunTestOnMainThreadLoop();
  }

  ash::ShelfModel* shelf_model() {
    return ash::test::ShellTestApi(ash::Shell::GetInstance()).shelf_model();
  }

  ash::ShelfID CreateAppShortcutLauncherItem(const std::string& name) {
    return controller_->CreateAppShortcutLauncherItem(
        name, controller_->model()->item_count());
  }

  const ash::ShelfItem& GetLastLauncherItem() {
    // Unless there are any panels, the item at index [count - 1] will be
    // the desired item.
    return shelf_model()->items()[shelf_model()->item_count() - 1];
  }

  const ash::ShelfItem& GetLastLauncherPanelItem() {
    // Panels show up on the right side of the shelf, so the desired item
    // will be the last one.
    return shelf_model()->items()[shelf_model()->item_count() - 1];
  }

  LauncherItemController* GetItemController(ash::ShelfID id) {
    return controller_->id_to_item_controller_map_[id];
  }

  // Returns the number of menu items, ignoring separators.
  int GetNumApplicationMenuItems(const ash::ShelfItem& item) {
    const int event_flags = 0;
    scoped_ptr<ash::ShelfMenuModel> menu(new LauncherApplicationMenuItemModel(
        controller_->GetApplicationList(item, event_flags)));
    int num_items = 0;
    for (int i = 0; i < menu->GetItemCount(); ++i) {
      if (menu->GetTypeAt(i) != ui::MenuModel::TYPE_SEPARATOR)
        ++num_items;
    }
    return num_items;
  }

  // Activate the shelf item with the given |id|.
  void ActivateShelfItem(int id) {
    shelf_->ActivateShelfItem(id);
  }

  ash::Shelf* shelf_;
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

class ShelfAppBrowserTest : public ExtensionBrowserTest {
 protected:
  ShelfAppBrowserTest() : shelf_(NULL), model_(NULL), controller_(NULL) {
  }

  virtual ~ShelfAppBrowserTest() {}

  virtual void RunTestOnMainThreadLoop() OVERRIDE {
    shelf_ = ash::Shelf::ForPrimaryDisplay();
    model_ = ash::test::ShellTestApi(ash::Shell::GetInstance()).shelf_model();
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
      extensions::LaunchContainer container,
      WindowOpenDisposition disposition) {
    EXPECT_TRUE(LoadExtension(test_data_dir_.AppendASCII(name)));

    ExtensionService* service = extensions::ExtensionSystem::Get(
        profile())->extension_service();
    const Extension* extension =
        service->GetExtensionById(last_loaded_extension_id(), false);
    EXPECT_TRUE(extension);

    OpenApplication(AppLaunchParams(profile(),
                                    extension,
                                    container,
                                    disposition));
    return extension;
  }

  ash::ShelfID CreateShortcut(const char* name) {
    ExtensionService* service = extensions::ExtensionSystem::Get(
        profile())->extension_service();
    LoadExtension(test_data_dir_.AppendASCII(name));

    // First get app_id.
    const Extension* extension =
        service->GetExtensionById(last_loaded_extension_id(), false);
    const std::string app_id = extension->id();

    // Then create a shortcut.
    int item_count = model_->item_count();
    ash::ShelfID shortcut_id = controller_->CreateAppShortcutLauncherItem(
        app_id,
        item_count);
    controller_->PersistPinnedState();
    EXPECT_EQ(++item_count, model_->item_count());
    const ash::ShelfItem& item = *model_->ItemByID(shortcut_id);
    EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
    return item.id;
  }

  void RemoveShortcut(ash::ShelfID id) {
    controller_->Unpin(id);
  }

  // Activate the shelf item with the given |id|.
  void ActivateShelfItem(int id) {
    shelf_->ActivateShelfItem(id);
  }

  ash::ShelfID PinFakeApp(const std::string& name) {
    return controller_->CreateAppShortcutLauncherItem(
        name, model_->item_count());
  }

  // Get the index of an item which has the given type.
  int GetIndexOfShelfItemType(ash::ShelfItemType type) {
    return model_->GetItemIndexForType(type);
  }

  // Try to rip off |item_index|.
  void RipOffItemIndex(int index,
                       aura::test::EventGenerator* generator,
                       ash::test::ShelfViewTestAPI* test,
                       RipOffCommand command) {
    ash::ShelfButton* button = test->GetButton(index);
    gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
    gfx::Point rip_off_point(start_point.x(), 0);
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
      base::MessageLoop::current()->RunUntilIdle();
      test->RunMessageLoopUntilAnimationsDone();
    }
  }

  ash::Shelf* shelf_;
  ash::ShelfModel* model_;
  ChromeLauncherController* controller_;

 private:

  DISALLOW_COPY_AND_ASSIGN(ShelfAppBrowserTest);
};

class ShelfAppBrowserTestNoDefaultBrowser : public ShelfAppBrowserTest {
 protected:
  ShelfAppBrowserTestNoDefaultBrowser() {}
  virtual ~ShelfAppBrowserTestNoDefaultBrowser() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ShelfAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(ShelfAppBrowserTestNoDefaultBrowser);
};

// Since the default for minimizing on click might change, I added both classes
// to either get the minimize on click or not.
class ShelfAppBrowserNoMinimizeOnClick : public LauncherPlatformAppBrowserTest {
 protected:
  ShelfAppBrowserNoMinimizeOnClick() {}
  virtual ~ShelfAppBrowserNoMinimizeOnClick() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    LauncherPlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kDisableMinimizeOnSecondLauncherItemClick);
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(ShelfAppBrowserNoMinimizeOnClick);
};

typedef LauncherPlatformAppBrowserTest ShelfAppBrowserMinimizeOnClick;

// Test that we can launch a platform app and get a running item.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, LaunchUnpinned) {
  int item_count = shelf_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window = CreateAppWindow(extension);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item = GetLastLauncherItem();
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);
  CloseAppWindow(window);
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());
}

// Test that we can launch a platform app that already has a shortcut.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, LaunchPinned) {
  int item_count = shelf_model()->item_count();

  // First get app_id.
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  const std::string app_id = extension->id();

  // Then create a shortcut.
  ash::ShelfID shortcut_id = CreateAppShortcutLauncherItem(app_id);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  ash::ShelfItem item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);

  // Open a window. Confirm the item is now running.
  AppWindow* window = CreateAppWindow(extension);
  ash::wm::ActivateWindow(window->GetNativeWindow());
  ASSERT_EQ(item_count, shelf_model()->item_count());
  item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);

  // Then close it, make sure there's still an item.
  CloseAppWindow(window);
  ASSERT_EQ(item_count, shelf_model()->item_count());
  item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, PinRunning) {
  // Run.
  int item_count = shelf_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window = CreateAppWindow(extension);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  ash::ShelfID id = item1.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);

  // Create a shortcut. The app item should be after it.
  ash::ShelfID foo_id = CreateAppShortcutLauncherItem("foo");
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  EXPECT_LT(shelf_model()->ItemIndexByID(foo_id),
            shelf_model()->ItemIndexByID(id));

  // Pin the app. The item should remain.
  controller_->Pin(id);
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item2 = *shelf_model()->ItemByID(id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item2.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item2.status);

  // New shortcuts should come after the item.
  ash::ShelfID bar_id = CreateAppShortcutLauncherItem("bar");
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  EXPECT_LT(shelf_model()->ItemIndexByID(id),
            shelf_model()->ItemIndexByID(bar_id));

  // Then close it, make sure the item remains.
  CloseAppWindow(window);
  ASSERT_EQ(item_count, shelf_model()->item_count());
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, UnpinRunning) {
  int item_count = shelf_model()->item_count();

  // First get app_id.
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  const std::string app_id = extension->id();

  // Then create a shortcut.
  ash::ShelfID shortcut_id = CreateAppShortcutLauncherItem(app_id);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  ash::ShelfItem item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);

  // Create a second shortcut. This will be needed to force the first one to
  // move once it gets unpinned.
  ash::ShelfID foo_id = CreateAppShortcutLauncherItem("foo");
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  EXPECT_LT(shelf_model()->ItemIndexByID(shortcut_id),
            shelf_model()->ItemIndexByID(foo_id));

  // Open a window. Confirm the item is now running.
  AppWindow* window = CreateAppWindow(extension);
  ash::wm::ActivateWindow(window->GetNativeWindow());
  ASSERT_EQ(item_count, shelf_model()->item_count());
  item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);

  // Unpin the app. The item should remain.
  controller_->Unpin(shortcut_id);
  ASSERT_EQ(item_count, shelf_model()->item_count());
  item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);
  // The item should have moved after the other shortcuts.
  EXPECT_GT(shelf_model()->ItemIndexByID(shortcut_id),
            shelf_model()->ItemIndexByID(foo_id));

  // Then close it, make sure the item's gone.
  CloseAppWindow(window);
  --item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
}

// Test that we can launch a platform app with more than one window.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, MultipleWindows) {
  int item_count = shelf_model()->item_count();

  // First run app.
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(extension);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  ash::ShelfID item_id = item1.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  EXPECT_EQ(2, GetNumApplicationMenuItems(item1));  // Title + 1 window

  // Add second window.
  AppWindow* window2 = CreateAppWindow(extension);
  // Confirm item stays.
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item2 = *shelf_model()->ItemByID(item_id);
  EXPECT_EQ(ash::STATUS_ACTIVE, item2.status);
  EXPECT_EQ(3, GetNumApplicationMenuItems(item2));  // Title + 2 windows

  // Close second window.
  CloseAppWindow(window2);
  // Confirm item stays.
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item3 = *shelf_model()->ItemByID(item_id);
  EXPECT_EQ(ash::STATUS_ACTIVE, item3.status);
  EXPECT_EQ(2, GetNumApplicationMenuItems(item3));  // Title + 1 window

  // Close first window.
  CloseAppWindow(window1);
  // Confirm item is removed.
  --item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, MultipleApps) {
  int item_count = shelf_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(extension1);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  ash::ShelfID item_id1 = item1.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);

  // Then run second app.
  const Extension* extension2 = LoadAndLaunchPlatformApp("launch_2",
                                                         "Launched");
  AppWindow* window2 = CreateAppWindow(extension2);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item2 = GetLastLauncherItem();
  ash::ShelfID item_id2 = item2.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item2.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item2.status);

  EXPECT_NE(item_id1, item_id2);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id1)->status);

  // Close second app.
  CloseAppWindow(window2);
  --item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  // First app should be active again.
  EXPECT_EQ(ash::STATUS_ACTIVE, shelf_model()->ItemByID(item_id1)->status);

  // Close first app.
  CloseAppWindow(window1);
  --item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
}

// Confirm that app windows can be reactivated by clicking their icons and that
// the correct activation order is maintained.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, WindowActivation) {
  int item_count = shelf_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(extension1);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  ash::ShelfID item_id1 = item1.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);

  // Then run second app.
  const Extension* extension2 = LoadAndLaunchPlatformApp("launch_2",
                                                         "Launched");
  AppWindow* window2 = CreateAppWindow(extension2);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item2 = GetLastLauncherItem();
  ash::ShelfID item_id2 = item2.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item2.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item2.status);

  EXPECT_NE(item_id1, item_id2);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id1)->status);

  // Activate first one.
  ActivateShelfItem(shelf_model()->ItemIndexByID(item_id1));
  EXPECT_EQ(ash::STATUS_ACTIVE, shelf_model()->ItemByID(item_id1)->status);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id2)->status);
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));

  // Activate second one.
  ActivateShelfItem(shelf_model()->ItemIndexByID(item_id2));
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id1)->status);
  EXPECT_EQ(ash::STATUS_ACTIVE, shelf_model()->ItemByID(item_id2)->status);
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));

  // Add window for app1. This will activate it.
  AppWindow* window1b = CreateAppWindow(extension1);
  ash::wm::ActivateWindow(window1b->GetNativeWindow());
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));

  // Activate launcher item for app1, this will activate the first app window.
  ActivateShelfItem(shelf_model()->ItemIndexByID(item_id1));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));
  ActivateShelfItem(shelf_model()->ItemIndexByID(item_id1));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));

  // Activate the second app again
  ActivateShelfItem(shelf_model()->ItemIndexByID(item_id2));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));

  // Activate the first app again
  ActivateShelfItem(shelf_model()->ItemIndexByID(item_id1));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));

  // Close second app.
  CloseAppWindow(window2);
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());
  // First app should be active again.
  EXPECT_EQ(ash::STATUS_ACTIVE, shelf_model()->ItemByID(item_id1)->status);

  // Close first app.
  CloseAppWindow(window1b);
  CloseAppWindow(window1);
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());
}

// Verify that ChromeLauncherController::CanInstall() returns true for ephemeral
// apps and false when the app is promoted to a regular installed app.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, InstallEphemeralApp) {
  int item_count = shelf_model()->item_count();

  // Sanity check to verify that ChromeLauncherController::CanInstall() returns
  // false for apps that are fully installed.
  const Extension* app = LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(app);
  CreateAppWindow(app);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& app_item = GetLastLauncherItem();
  ash::ShelfID app_id = app_item.id;
  EXPECT_FALSE(controller_->CanInstall(app_id));

  // Add an ephemeral app.
  const Extension* ephemeral_app = InstallEphemeralAppWithSourceAndFlags(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("launch_2"),
      1,
      extensions::Manifest::INTERNAL,
      Extension::NO_FLAGS);
  ASSERT_TRUE(ephemeral_app);
  CreateAppWindow(ephemeral_app);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& ephemeral_item = GetLastLauncherItem();
  ash::ShelfID ephemeral_id = ephemeral_item.id;

  // Verify that the shelf item for the ephemeral app can be installed.
  EXPECT_TRUE(controller_->CanInstall(ephemeral_id));

  // Promote the ephemeral app to a regular installed app.
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  service->PromoteEphemeralApp(ephemeral_app, false);

  // Verify that the shelf item for the app can no longer be installed.
  EXPECT_FALSE(controller_->CanInstall(ephemeral_id));
}

// Confirm that Click behavior for app windows is correnct.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserNoMinimizeOnClick, AppClickBehavior) {
  // Launch a platform app and create a window for it.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(extension1);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  // Confirm that a controller item was created and is the correct state.
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  LauncherItemController* item1_controller = GetItemController(item1.id);
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  EXPECT_EQ(LauncherItemController::TYPE_APP, item1_controller->type());
  // Clicking the item should have no effect.
  TestEvent click_event(ui::ET_MOUSE_PRESSED);
  item1_controller->ItemSelected(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  // Minimize the window and confirm that the controller item is updated.
  window1->GetBaseWindow()->Minimize();
  EXPECT_FALSE(window1->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);
  // Clicking the item should activate the window.
  item1_controller->ItemSelected(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  // Maximizing a window should preserve state after minimize + click.
  window1->GetBaseWindow()->Maximize();
  window1->GetBaseWindow()->Minimize();
  item1_controller->ItemSelected(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1->GetBaseWindow()->IsMaximized());
}

// Confirm the minimizing click behavior for apps.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserMinimizeOnClick,
                       PackagedAppClickBehaviorInMinimizeMode) {
  // Launch one platform app and create a window for it.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(extension1);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());

  // Confirm that a controller item was created and is the correct state.
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  LauncherItemController* item1_controller = GetItemController(item1.id);
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  EXPECT_EQ(LauncherItemController::TYPE_APP, item1_controller->type());
  // Since it is already active, clicking it should minimize.
  TestEvent click_event(ui::ET_MOUSE_PRESSED);
  item1_controller->ItemSelected(click_event);
  EXPECT_FALSE(window1->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1->GetBaseWindow()->IsMinimized());
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);
  // Clicking the item again should activate the window again.
  item1_controller->ItemSelected(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  // Maximizing a window should preserve state after minimize + click.
  window1->GetBaseWindow()->Maximize();
  window1->GetBaseWindow()->Minimize();
  item1_controller->ItemSelected(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1->GetBaseWindow()->IsMaximized());
  window1->GetBaseWindow()->Restore();
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window1->GetBaseWindow()->IsMaximized());

  // Creating a second window of the same type should change the behavior so
  // that a click does not change the activation state.
  AppWindow* window1a = CreateAppWindow(extension1);
  EXPECT_TRUE(window1a->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1a->GetBaseWindow()->IsActive());
  // The first click does nothing.
  item1_controller->ItemSelected(click_event);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1a->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window1a->GetBaseWindow()->IsActive());
  // The second neither.
  item1_controller->ItemSelected(click_event);
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
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow::CreateParams params;
  params.window_type = AppWindow::WINDOW_TYPE_PANEL;
  params.focused = false;
  AppWindow* panel = CreateAppWindowFromParams(extension1, params);
  EXPECT_TRUE(panel->GetNativeWindow()->IsVisible());
  // Panels should not be active by default.
  EXPECT_FALSE(panel->GetBaseWindow()->IsActive());
  // Confirm that a controller item was created and is the correct state.
  const ash::ShelfItem& item1 = GetLastLauncherPanelItem();
  LauncherItemController* item1_controller = GetItemController(item1.id);
  EXPECT_EQ(ash::TYPE_APP_PANEL, item1.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);
  EXPECT_EQ(LauncherItemController::TYPE_APP_PANEL, item1_controller->type());
  // Click the item and confirm that the panel is activated.
  TestEvent click_event(ui::ET_MOUSE_PRESSED);
  item1_controller->ItemSelected(click_event);
  EXPECT_TRUE(panel->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
  // Click the item again and confirm that the panel is minimized.
  item1_controller->ItemSelected(click_event);
  EXPECT_TRUE(panel->GetBaseWindow()->IsMinimized());
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);
  // Click the item again and confirm that the panel is activated.
  item1_controller->ItemSelected(click_event);
  EXPECT_TRUE(panel->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(panel->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, BrowserActivation) {
  int item_count = shelf_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  CreateAppWindow(extension1);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  ash::ShelfID item_id1 = item1.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);

  ash::wm::ActivateWindow(browser()->window()->GetNativeWindow());
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id1)->status);
}

// Test that opening an app sets the correct icon
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, SetIcon) {
  TestAppWindowRegistryObserver test_observer(browser()->profile());

  // Enable experimental APIs to allow panel creation.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableExperimentalExtensionApis);

  int base_shelf_item_count = shelf_model()->item_count();
  ExtensionTestMessageListener completed_listener("Completed", false);
  LoadAndLaunchPlatformApp("app_icon", "Launched");
  ASSERT_TRUE(completed_listener.WaitUntilSatisfied());

  // Now wait until the WebContent has decoded the icons and chrome has
  // processed it. This needs to be in a loop since the renderer runs in a
  // different process.
  while (test_observer.icon_updates() < 3) {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // This test creates one app window and one panel window.
  int shelf_item_count = shelf_model()->item_count();
  ASSERT_EQ(base_shelf_item_count + 2, shelf_item_count);
  // The Panel will be the last item, the app second-to-last.
  const ash::ShelfItem& app_item =
      shelf_model()->items()[shelf_item_count - 2];
  const ash::ShelfItem& panel_item =
      shelf_model()->items()[shelf_item_count - 1];
  const LauncherItemController* app_item_controller =
      GetItemController(app_item.id);
  const LauncherItemController* panel_item_controller =
      GetItemController(panel_item.id);
  // Icons for Apps are set by the AppWindowLauncherController, so
  // image_set_by_controller() should be set.
  EXPECT_TRUE(app_item_controller->image_set_by_controller());
  EXPECT_TRUE(panel_item_controller->image_set_by_controller());
  // Ensure icon heights are correct (see test.js in app_icon/ test directory)
  EXPECT_EQ(ash::kShelfSize, app_item.image.height());
  EXPECT_EQ(64, panel_item.image.height());
}

// Test that we can launch an app with a shortcut.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchPinned) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);
  ActivateShelfItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);
  WebContents* tab = tab_strip->GetActiveWebContents();
  content::WebContentsDestroyedWatcher destroyed_watcher(tab);
  browser()->tab_strip_model()->CloseSelectedTabs();
  destroyed_watcher.Wait();
  EXPECT_EQ(--tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);
}

// Launch the app first and then create the shortcut.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchUnpinned) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB,
                         NEW_FOREGROUND_TAB);
  EXPECT_EQ(++tab_count, tab_strip->count());
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);
  WebContents* tab = tab_strip->GetActiveWebContents();
  content::WebContentsDestroyedWatcher destroyed_watcher(tab);
  browser()->tab_strip_model()->CloseSelectedTabs();
  destroyed_watcher.Wait();
  EXPECT_EQ(--tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);
}

// Launches an app in the background and then tries to open it. This is test for
// a crash we had.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchInBackground) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB,
                         NEW_BACKGROUND_TAB);
  EXPECT_EQ(++tab_count, tab_strip->count());
  ChromeLauncherController::instance()->LaunchApp(last_loaded_extension_id(),
                                                  ash::LAUNCH_FROM_UNKNOWN,
                                                  0);
}

// Confirm that clicking a icon for an app running in one of 2 maxmized windows
// activates the right window.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchMaximized) {
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

  ash::ShelfID shortcut_id = CreateShortcut("app1");
  ActivateShelfItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);

  window1->Show();
  window1_state->Activate();
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut_id)).status);

  ActivateShelfItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);
}

// Activating the same app multiple times should launch only a single copy.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, ActivateApp) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app1"));

  ChromeLauncherController::instance()->ActivateApp(extension->id(),
                                                    ash::LAUNCH_FROM_UNKNOWN,
                                                    0);
  EXPECT_EQ(++tab_count, tab_strip->count());
  ChromeLauncherController::instance()->ActivateApp(extension->id(),
                                                    ash::LAUNCH_FROM_UNKNOWN,
                                                    0);
  EXPECT_EQ(tab_count, tab_strip->count());
}

// Launching the same app multiple times should launch a copy for each call.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchApp) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app1"));

  ChromeLauncherController::instance()->LaunchApp(extension->id(),
                                                  ash::LAUNCH_FROM_UNKNOWN,
                                                  0);
  EXPECT_EQ(++tab_count, tab_strip->count());
  ChromeLauncherController::instance()->LaunchApp(extension->id(),
                                                  ash::LAUNCH_FROM_UNKNOWN,
                                                  0);
  EXPECT_EQ(++tab_count, tab_strip->count());
}

// Launch 2 apps and toggle which is active.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, MultipleApps) {
  int item_count = model_->item_count();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::ShelfID shortcut1 = CreateShortcut("app1");
  EXPECT_EQ(++item_count, model_->item_count());
  ash::ShelfID shortcut2 = CreateShortcut("app2");
  EXPECT_EQ(++item_count, model_->item_count());

  // Launch first app.
  ActivateShelfItem(model_->ItemIndexByID(shortcut1));
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* tab1 = tab_strip->GetActiveWebContents();
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut1)).status);

  // Launch second app.
  ActivateShelfItem(model_->ItemIndexByID(shortcut2));
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* tab2 = tab_strip->GetActiveWebContents();
  ASSERT_NE(tab1, tab2);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut2)).status);

  // Reactivate first app.
  ActivateShelfItem(model_->ItemIndexByID(shortcut1));
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
  ActivateShelfItem(model_->ItemIndexByID(shortcut1));
  EXPECT_EQ(tab_count, tab_strip->count());
  EXPECT_EQ(tab_strip->GetActiveWebContents(), tab1);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut2)).status);

  // And second again. This time the second tab should become active.
  ActivateShelfItem(model_->ItemIndexByID(shortcut2));
  EXPECT_EQ(tab_count, tab_strip->count());
  EXPECT_EQ(tab_strip->GetActiveWebContents(), tab2);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut2)).status);
}

// Confirm that a page can be navigated from and to while maintaining the
// correct running state.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, Navigation) {
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);
  ActivateShelfItem(model_->ItemIndexByID(shortcut_id));
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

// Confirm that a tab can be moved between browsers while maintaining the
// correct running state.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, TabDragAndDrop) {
  TabStripModel* tab_strip_model1 = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model1->count());
  int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_TRUE(browser_index >= 0);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Create a shortcut for app1.
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->items()[browser_index].status);
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);

  // Activate app1 and check its item status.
  ActivateShelfItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(2, tab_strip_model1->count());
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[browser_index].status);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);

  // Create a new browser with blank tab.
  Browser* browser2 = CreateBrowser(profile());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model2->count());
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->items()[browser_index].status);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut_id)).status);

  // Detach a tab at index 1 (app1) from |tab_strip_model1| and insert it as an
  // active tab at index 1 to |tab_strip_model2|.
  content::WebContents* detached_tab = tab_strip_model1->DetachWebContentsAt(1);
  tab_strip_model2->InsertWebContentsAt(1,
                                        detached_tab,
                                        TabStripModel::ADD_ACTIVE);
  EXPECT_EQ(1, tab_strip_model1->count());
  EXPECT_EQ(2, tab_strip_model2->count());
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[browser_index].status);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);

  tab_strip_model1->CloseAllTabs();
  tab_strip_model2->CloseAllTabs();
}

IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, MultipleOwnedTabs) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  ActivateShelfItem(model_->ItemIndexByID(shortcut_id));
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
  ActivateShelfItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(tab_strip->GetActiveWebContents(), first_tab);
}

IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, RefocusFilter) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  ActivateShelfItem(model_->ItemIndexByID(shortcut_id));
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
  ActivateShelfItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(tab_strip->GetActiveWebContents(), first_tab);
}

IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, RefocusFilterLaunch) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::ShelfID shortcut_id = CreateShortcut("app1");
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
  ActivateShelfItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* second_tab = tab_strip->GetActiveWebContents();
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  EXPECT_NE(first_tab, second_tab);
  EXPECT_EQ(tab_strip->GetActiveWebContents(), second_tab);
}

// Check the launcher activation state for applications and browser.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, ActivationStateCheck) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  // Get the browser item index
  int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_TRUE(browser_index >= 0);

  // Even though we are just comming up, the browser should be active.
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->items()[browser_index].status);

  ash::ShelfID shortcut_id = CreateShortcut("app1");
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
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, AsyncActivationStateCheck) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ash::ShelfID shortcut_id = CreateShortcut("app1");
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
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
    WindowedAppDoesNotAddToBrowser) {
  // Get the number of items in the browser menu.
  size_t items = NumberOfDetectedLauncherBrowsers(false);
  size_t running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(0u, items);
  EXPECT_EQ(0u, running_browser);

  LoadAndLaunchExtension(
      "app1", extensions::LAUNCH_CONTAINER_WINDOW, NEW_WINDOW);

  // No new browser should get detected, even though one more is running.
  EXPECT_EQ(0u, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(++running_browser, chrome::GetTotalBrowserCount());

  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB, NEW_WINDOW);

  // A new browser should get detected and one more should be running.
  EXPECT_EQ(NumberOfDetectedLauncherBrowsers(false), 1u);
  EXPECT_EQ(++running_browser, chrome::GetTotalBrowserCount());
}

// Checks the functionality to enumerate all browsers vs. all tabs.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       EnumerateALlBrowsersAndTabs) {
  // Create at least one browser.
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB, NEW_WINDOW);
  size_t browsers = NumberOfDetectedLauncherBrowsers(false);
  size_t tabs = NumberOfDetectedLauncherBrowsers(true);

  // Create a second browser.
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB, NEW_WINDOW);

  EXPECT_EQ(++browsers, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(++tabs, NumberOfDetectedLauncherBrowsers(true));

  // Create only a tab.
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB,
                         NEW_FOREGROUND_TAB);

  EXPECT_EQ(browsers, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(++tabs, NumberOfDetectedLauncherBrowsers(true));
}

// Check that the keyboard activation of a launcher item tabs properly through
// the items at hand.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, AltNumberTabsTabbing) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ash::ShelfID shortcut_id = CreateShortcut("app");
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
  ActivateShelfItem(shortcut_index);
  EXPECT_EQ(content1, tab_strip->GetActiveWebContents());
  ActivateShelfItem(shortcut_index);
  EXPECT_EQ(content1, tab_strip->GetActiveWebContents());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* content2 = tab_strip->GetActiveWebContents();

  EXPECT_EQ(content2, browser()->tab_strip_model()->GetActiveWebContents());
  ActivateShelfItem(shortcut_index);
  EXPECT_EQ(content1, browser()->tab_strip_model()->GetActiveWebContents());
  ActivateShelfItem(shortcut_index);
  EXPECT_EQ(content2, browser()->tab_strip_model()->GetActiveWebContents());
}

// Check that the keyboard activation of a launcher item tabs properly through
// the items at hand.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest,
                       AltNumberAppsTabbing) {
  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  ui::BaseWindow* window1 = CreateAppWindow(extension1)->GetBaseWindow();
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  ash::ShelfID app_id = item1.id;
  int app_index = shelf_model()->ItemIndexByID(app_id);

  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item1.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item1.status);

  const Extension* extension2 = LoadAndLaunchPlatformApp("launch_2",
                                                         "Launched");
  ui::BaseWindow* window2 = CreateAppWindow(extension2)->GetBaseWindow();

  // By now the browser should be active. Issue Alt keystrokes several times to
  // see that we stay on that application.
  EXPECT_TRUE(window2->IsActive());
  ActivateShelfItem(app_index);
  EXPECT_TRUE(window1->IsActive());
  ActivateShelfItem(app_index);
  EXPECT_TRUE(window1->IsActive());

  ui::BaseWindow* window1a = CreateAppWindow(extension1)->GetBaseWindow();

  EXPECT_TRUE(window1a->IsActive());
  EXPECT_FALSE(window1->IsActive());
  ActivateShelfItem(app_index);
  EXPECT_TRUE(window1->IsActive());
  ActivateShelfItem(app_index);
  EXPECT_TRUE(window1a->IsActive());
}

// Test that we can launch a platform app panel and get a running item.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, LaunchPanelWindow) {
  int item_count = shelf_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow::CreateParams params;
  params.window_type = AppWindow::WINDOW_TYPE_PANEL;
  params.focused = false;
  AppWindow* window = CreateAppWindowFromParams(extension, params);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item = GetLastLauncherPanelItem();
  EXPECT_EQ(ash::TYPE_APP_PANEL, item.type);
  // Opening a panel does not activate it.
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);
  CloseAppWindow(window);
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());
}

// Test that we get correct shelf presence with hidden app windows.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, HiddenAppWindows) {
  int item_count = shelf_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow::CreateParams params;

  // Create a hidden window.
  params.hidden = true;
  AppWindow* window_1 = CreateAppWindowFromParams(extension, params);
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Create a visible window.
  params.hidden = false;
  AppWindow* window_2 = CreateAppWindowFromParams(extension, params);
  ++item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Minimize the visible window.
  window_2->Minimize();
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Hide the visible window.
  window_2->Hide();
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Show the originally hidden window.
  window_1->Show(AppWindow::SHOW_ACTIVE);
  ++item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Close the originally hidden window.
  CloseAppWindow(window_1);
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());
}

// Test attention states of windows.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, WindowAttentionStatus) {
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow::CreateParams params;
  params.window_type = AppWindow::WINDOW_TYPE_PANEL;
  params.focused = false;
  AppWindow* panel = CreateAppWindowFromParams(extension, params);
  EXPECT_TRUE(panel->GetNativeWindow()->IsVisible());
  // Panels should not be active by default.
  EXPECT_FALSE(panel->GetBaseWindow()->IsActive());
  // Confirm that a controller item was created and is the correct state.
  const ash::ShelfItem& item = GetLastLauncherPanelItem();
  LauncherItemController* item_controller = GetItemController(item.id);
  EXPECT_EQ(ash::TYPE_APP_PANEL, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);
  EXPECT_EQ(LauncherItemController::TYPE_APP_PANEL, item_controller->type());

  // App windows should go to attention state.
  panel->GetNativeWindow()->SetProperty(aura::client::kDrawAttentionKey, true);
  EXPECT_EQ(ash::STATUS_ATTENTION, item.status);

  // Click the item and confirm that the panel is activated.
  TestEvent click_event(ui::ET_MOUSE_PRESSED);
  item_controller->ItemSelected(click_event);
  EXPECT_TRUE(panel->GetBaseWindow()->IsActive());
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);

  // Active windows don't show attention.
  panel->GetNativeWindow()->SetProperty(aura::client::kDrawAttentionKey, true);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);
}

// Checks that the browser Alt "tabbing" is properly done.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       AltNumberBrowserTabbing) {
  // Get the number of items in the browser menu.
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  // The first activation should create a browser at index 1 (App List @ 0).
  shelf_->ActivateShelfItem(1);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  // A second activation should not create a new instance.
  shelf_->ActivateShelfItem(1);
  Browser* browser1 = chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
  EXPECT_TRUE(browser1);
  aura::Window* window1 = browser1->window()->GetNativeWindow();
  Browser* browser2 = CreateBrowser(profile());
  aura::Window* window2 = browser2->window()->GetNativeWindow();

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_NE(window1, window2);
  EXPECT_EQ(window2, ash::wm::GetActiveWindow());

  // Activate multiple times the switcher to see that the windows get activated.
  shelf_->ActivateShelfItem(1);
  EXPECT_EQ(window1, ash::wm::GetActiveWindow());
  shelf_->ActivateShelfItem(1);
  EXPECT_EQ(window2, ash::wm::GetActiveWindow());

  // Create a third browser - make sure that we do not toggle simply between
  // two windows.
  Browser* browser3 = CreateBrowser(profile());
  aura::Window* window3 = browser3->window()->GetNativeWindow();

  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_NE(window1, window3);
  EXPECT_NE(window2, window3);
  EXPECT_EQ(window3, ash::wm::GetActiveWindow());

  shelf_->ActivateShelfItem(1);
  EXPECT_EQ(window1, ash::wm::GetActiveWindow());
  shelf_->ActivateShelfItem(1);
  EXPECT_EQ(window2, ash::wm::GetActiveWindow());
  shelf_->ActivateShelfItem(1);
  EXPECT_EQ(window3, ash::wm::GetActiveWindow());
  shelf_->ActivateShelfItem(1);
  EXPECT_EQ(window1, ash::wm::GetActiveWindow());

  // Create anther app and make sure that none of our browsers is active.
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB, NEW_WINDOW);
  EXPECT_NE(window1, ash::wm::GetActiveWindow());
  EXPECT_NE(window2, ash::wm::GetActiveWindow());

  // After activation our browser should be active again.
  shelf_->ActivateShelfItem(1);
  EXPECT_EQ(window1, ash::wm::GetActiveWindow());
}

// Checks that after a session restore, we do not start applications on an
// activation.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, ActivateAfterSessionRestore) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Create a known application.
  ash::ShelfID shortcut_id = CreateShortcut("app1");

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
  LauncherItemController* item_controller =
      controller_->GetLauncherItemController(shortcut_id);
  item_controller->ItemSelected(ui::KeyEvent(ui::ET_KEY_RELEASED,
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
// TODO(skuhne): Test is flaky with a real compositor: crbug.com/331924
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, DISABLED_DragAndDrop) {
  // Get a number of interfaces we need.
  aura::test::EventGenerator generator(
      ash::Shell::GetPrimaryRootWindow(), gfx::Point());
  ash::test::ShelfViewTestAPI test(
      ash::test::ShelfTestAPI(shelf_).shelf_view());
  AppListService* service = AppListService::Get(chrome::GetActiveDesktop());

  // There should be two items in our launcher by this time.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(service->IsAppListVisible());

  // Open the app list menu and check that the drag and drop host was set.
  gfx::Rect app_list_bounds =
      test.shelf_view()->GetAppListButtonView()->GetBoundsInScreen();
  generator.MoveMouseTo(app_list_bounds.CenterPoint().x(),
                        app_list_bounds.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  generator.ClickLeftButton();

  EXPECT_TRUE(service->IsAppListVisible());
  app_list::AppsGridView* grid_view =
      ash::test::AppListControllerTestApi(ash::Shell::GetInstance()).
          GetRootGridView();
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

  // Drag the item into the shelf and check that a new item gets created.
  const views::ViewModel* vm_shelf = test.shelf_view()->view_model_for_test();
  views::View* shelf1 = vm_shelf->view_at(1);
  gfx::Rect bounds_shelf_1 = shelf1->GetBoundsInScreen();
  generator.MoveMouseTo(bounds_shelf_1.CenterPoint().x(),
                        bounds_shelf_1.CenterPoint().y());
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
  generator.MoveMouseTo(bounds_shelf_1.CenterPoint().x(),
                        bounds_shelf_1.CenterPoint().y());
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
  generator.MoveMouseTo(bounds_shelf_1.CenterPoint().x(),
                        bounds_shelf_1.CenterPoint().y());
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
  generator.MoveMouseTo(bounds_shelf_1.CenterPoint().x(),
                        bounds_shelf_1.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  // Issue an ESC and see that the operation gets cancelled.
  generator.PressKey(ui::VKEY_ESCAPE, 0);
  generator.ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_FALSE(grid_view->dragging());
  EXPECT_FALSE(grid_view->has_dragged_view());
  generator.ReleaseLeftButton();
}

#if !defined(OS_WIN)
// Used to test drag & drop an item between app list and shelf with multi
// display environment.
class ShelfAppBrowserTestWithMultiMonitor
    : public ShelfAppBrowserTestNoDefaultBrowser {
 protected:
  ShelfAppBrowserTestWithMultiMonitor() {}
  virtual ~ShelfAppBrowserTestWithMultiMonitor() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ShelfAppBrowserTestNoDefaultBrowser::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "800x600,801+0-800x600");
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(ShelfAppBrowserTestWithMultiMonitor);
};

// Do basic drag and drop interaction tests between the application list and
// the launcher in the secondary monitor.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestWithMultiMonitor,
    BasicDragAndDrop) {
  // Get a number of interfaces we need.
  DCHECK_EQ(ash::Shell::GetAllRootWindows().size(), 2U);
  aura::Window* secondary_root_window = ash::Shell::GetAllRootWindows()[1];
  ash::Shelf* secondary_shelf = ash::Shelf::ForWindow(secondary_root_window);

  aura::test::EventGenerator generator(secondary_root_window, gfx::Point());
  ash::test::ShelfViewTestAPI test(
      ash::test::ShelfTestAPI(secondary_shelf).shelf_view());
  AppListService* service = AppListService::Get(chrome::GetActiveDesktop());

  // There should be two items in our shelf by this time.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(service->IsAppListVisible());

  // Open the app list menu and check that the drag and drop host was set.
  gfx::Rect app_list_bounds =
      test.shelf_view()->GetAppListButtonView()->GetBoundsInScreen();
  gfx::Display display =
      ash::Shell::GetScreen()->GetDisplayNearestWindow(secondary_root_window);
  const gfx::Point& origin = display.bounds().origin();
  app_list_bounds.Offset(-origin.x(), -origin.y());

  generator.MoveMouseTo(app_list_bounds.CenterPoint().x(),
                        app_list_bounds.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  generator.ClickLeftButton();

  EXPECT_TRUE(service->IsAppListVisible());
  app_list::AppsGridView* grid_view =
      ash::test::AppListControllerTestApi(ash::Shell::GetInstance()).
          GetRootGridView();
  ASSERT_TRUE(grid_view);
  ASSERT_TRUE(grid_view->has_drag_and_drop_host_for_test());

  // There should be 2 items in our application list.
  const views::ViewModel* vm_grid = grid_view->view_model_for_test();
  EXPECT_EQ(2, vm_grid->view_size());

  // Drag an app list item which does not exist yet in the shelf.
  // Keeping it dragged, see that a new item gets created.
  // Continuing to drag it out should remove it again.

  // Get over item #1 of the application list and press the mouse button.
  views::View* item1 = vm_grid->view_at(1);
  gfx::Rect bounds_grid_1 = item1->GetBoundsInScreen();
  bounds_grid_1.Offset(-origin.x(), -origin.y());
  generator.MoveMouseTo(bounds_grid_1.CenterPoint().x(),
                        bounds_grid_1.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  generator.PressLeftButton();

  EXPECT_FALSE(grid_view->forward_events_to_drag_and_drop_host_for_test());

  // Drag the item into the shelf and check that a new item gets created.
  const views::ViewModel* vm_shelf = test.shelf_view()->view_model_for_test();
  views::View* shelf1 = vm_shelf->view_at(1);
  gfx::Rect bounds_shelf_1 = shelf1->GetBoundsInScreen();
  bounds_shelf_1.Offset(-origin.x(), -origin.y());
  generator.MoveMouseTo(bounds_shelf_1.CenterPoint().x(),
                        bounds_shelf_1.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();

  // Check that a new item got created.
  EXPECT_EQ(3, model_->item_count());
  EXPECT_TRUE(grid_view->forward_events_to_drag_and_drop_host_for_test());

  // Move it to an empty slot on grid_view.
  gfx::Rect empty_slot_rect = bounds_grid_1;
  empty_slot_rect.Offset(0, bounds_grid_1.height());
  generator.MoveMouseTo(empty_slot_rect.CenterPoint().x(),
                        empty_slot_rect.CenterPoint().y());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(grid_view->forward_events_to_drag_and_drop_host_for_test());

  // Dropping it should keep the shelf as it originally was.
  generator.ReleaseLeftButton();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, model_->item_count());
}
#endif

// Do tests for removal of items from the shelf by dragging.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, DragOffShelf) {
  aura::test::EventGenerator generator(
      ash::Shell::GetPrimaryRootWindow(), gfx::Point());
  ash::test::ShelfViewTestAPI test(
      ash::test::ShelfTestAPI(shelf_).shelf_view());
  test.SetAnimationDuration(1);  // Speed up animations for test.
  // Create a known application and check that we have 3 items in the shelf.
  CreateShortcut("app1");
  test.RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(3, model_->item_count());

  // Test #1: Ripping out the browser item should not change anything.
  int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_LE(0, browser_index);
  RipOffItemIndex(browser_index, &generator, &test, RIP_OFF_ITEM);
  // => It should not have been removed and the location should be unchanged.
  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(browser_index,
            GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT));
  // Make sure that the hide state has been unset after the snap back animation
  // finished.
  ash::ShelfButton* button = test.GetButton(browser_index);
  EXPECT_FALSE(button->state() & ash::ShelfButton::STATE_HIDDEN);

  // Test #2: Ripping out the application and canceling the operation should
  // not change anything.
  int app_index = GetIndexOfShelfItemType(ash::TYPE_APP_SHORTCUT);
  EXPECT_LE(0, app_index);
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM_AND_CANCEL);
  // => It should not have been removed and the location should be unchanged.
  ASSERT_EQ(3, model_->item_count());
  EXPECT_EQ(app_index, GetIndexOfShelfItemType(ash::TYPE_APP_SHORTCUT));

  // Test #3: Ripping out the application and moving it back in should not
  // change anything.
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM_AND_RETURN);
  // => It should not have been removed and the location should be unchanged.
  ASSERT_EQ(3, model_->item_count());
  // Through the operation the index might have changed.
  app_index = GetIndexOfShelfItemType(ash::TYPE_APP_SHORTCUT);

  // Test #4: Ripping out the application should remove the item.
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM);
  // => It should not have been removed and the location should be unchanged.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(-1, GetIndexOfShelfItemType(ash::TYPE_APP_SHORTCUT));

  // Test #5: Uninstalling an application while it is being ripped off should
  // not crash.
  ash::ShelfID app_id = CreateShortcut("app2");
  test.RunMessageLoopUntilAnimationsDone();
  int app2_index = GetIndexOfShelfItemType(ash::TYPE_APP_SHORTCUT);
  EXPECT_EQ(3, model_->item_count());  // And it remains that way.
  RipOffItemIndex(app2_index,
                  &generator,
                  &test,
                  RIP_OFF_ITEM_AND_DONT_RELEASE_MOUSE);
  RemoveShortcut(app_id);
  test.RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(2, model_->item_count());  // The item should now be gone.
  generator.ReleaseLeftButton();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, model_->item_count());  // And it remains that way.
  EXPECT_EQ(-1, GetIndexOfShelfItemType(ash::TYPE_APP_SHORTCUT));

  // Test #6: Ripping out the application when the overflow button exists.
  // After ripping out, overflow button should be removed.
  int items_added = 0;
  EXPECT_FALSE(test.IsOverflowButtonVisible());

  // Create fake app shortcuts until overflow button is created.
  while (!test.IsOverflowButtonVisible()) {
    std::string fake_app_id = base::StringPrintf("fake_app_%d", items_added);
    PinFakeApp(fake_app_id);
    test.RunMessageLoopUntilAnimationsDone();

    ++items_added;
    ASSERT_LT(items_added, 10000);
  }
  // Make one more item after creating a overflow button.
  std::string fake_app_id = base::StringPrintf("fake_app_%d", items_added);
  PinFakeApp(fake_app_id);
  test.RunMessageLoopUntilAnimationsDone();

  int total_count = model_->item_count();
  app_index = GetIndexOfShelfItemType(ash::TYPE_APP_SHORTCUT);
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM);
  // When an item is ripped off from the shelf that has overflow button
  // (see crbug.com/3050787), it was hidden accidentally and was then
  // suppressing any further events. If handled correctly the operation will
  // however correctly done and the item will get removed (as well as the
  // overflow button).
  EXPECT_EQ(total_count - 1, model_->item_count());
  EXPECT_TRUE(test.IsOverflowButtonVisible());

  // Rip off again and the overflow button should has disappeared.
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM);
  EXPECT_EQ(total_count - 2, model_->item_count());
  EXPECT_FALSE(test.IsOverflowButtonVisible());
}

// Check that clicking on an app shelf item launches a new browser.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, ClickItem) {
  // Get a number of interfaces we need.
  aura::test::EventGenerator generator(
      ash::Shell::GetPrimaryRootWindow(), gfx::Point());
  ash::test::ShelfViewTestAPI test(
      ash::test::ShelfTestAPI(shelf_).shelf_view());
  AppListService* service = AppListService::Get(chrome::GetActiveDesktop());
  // There should be two items in our shelf by this time.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(service->IsAppListVisible());

  // Open the app list menu and check that the drag and drop host was set.
  gfx::Rect app_list_bounds =
      test.shelf_view()->GetAppListButtonView()->GetBoundsInScreen();
  generator.MoveMouseTo(app_list_bounds.CenterPoint().x(),
                        app_list_bounds.CenterPoint().y());
  generator.ClickLeftButton();
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(service->IsAppListVisible());
  app_list::AppsGridView* grid_view =
      ash::test::AppListControllerTestApi(ash::Shell::GetInstance()).
          GetRootGridView();
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
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       BrowserShortcutLauncherItemController) {
  LauncherItemController* item_controller =
      controller_->GetBrowserShortcutLauncherItemController();

  // Get the number of browsers.
  size_t running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(0u, running_browser);
  EXPECT_FALSE(item_controller->IsOpen());

  // Activate. This creates new browser
  item_controller->Activate(ash::LAUNCH_FROM_UNKNOWN);
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
  item_controller->Activate(ash::LAUNCH_FROM_UNKNOWN);
  running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(1u, running_browser);
  EXPECT_TRUE(item_controller->IsOpen());
  EXPECT_FALSE(window_state->IsMinimized());
}

// Check that GetShelfIDForWindow() returns |ShelfID| of the active tab.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, MatchingShelfIDandActiveTab) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(2, model_->item_count());

  aura::Window* window = browser()->window()->GetNativeWindow();

  int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  ash::ShelfID browser_id = model_->items()[browser_index].id;
  EXPECT_EQ(browser_id, ash::GetShelfIDForWindow(window));

  ash::ShelfID app_id = CreateShortcut("app1");
  EXPECT_EQ(3, model_->item_count());

  // Creates a new tab for "app1" and checks that GetShelfIDForWindow()
  // returns |ShelfID| of "app1".
  ActivateShelfItem(model_->ItemIndexByID(app_id));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(app_id, ash::GetShelfIDForWindow(window));

  // Makes tab at index 0(NTP) as an active tab and checks that
  // GetShelfIDForWindow() returns |ShelfID| of browser shortcut.
  browser()->tab_strip_model()->ActivateTabAt(0, false);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(browser_id, ash::GetShelfIDForWindow(window));
}

IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, OverflowBubble) {
  // Make sure to have a browser window
  chrome::NewTab(browser());

  // No overflow yet.
  EXPECT_FALSE(shelf_->IsShowingOverflowBubble());

  ash::test::ShelfViewTestAPI test(
      ash::test::ShelfTestAPI(shelf_).shelf_view());

  int items_added = 0;
  while (!test.IsOverflowButtonVisible()) {
    std::string fake_app_id = base::StringPrintf("fake_app_%d", items_added);
    PinFakeApp(fake_app_id);

    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Now show overflow bubble.
  test.ShowOverflowBubble();
  EXPECT_TRUE(shelf_->IsShowingOverflowBubble());

  // Unpin first pinned app and there should be no crash.
  controller_->UnpinAppWithID(std::string("fake_app_0"));

  test.RunMessageLoopUntilAnimationsDone();
  EXPECT_FALSE(shelf_->IsShowingOverflowBubble());
}

// Check that a windowed V1 application can navigate away from its domain, but
// still gets detected properly.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, V1AppNavigation) {
  // We assume that the web store is always there (which it apparently is).
  controller_->PinAppWithID(extension_misc::kWebStoreAppId);
  ash::ShelfID id = controller_->GetShelfIDForAppID(
      extension_misc::kWebStoreAppId);
  ASSERT_NE(0, id);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->ItemByID(id)->status);

  // Create a windowed application.
  AppLaunchParams params(
      profile(),
      controller_->GetExtensionForAppID(extension_misc::kWebStoreAppId),
      0,
      chrome::HOST_DESKTOP_TYPE_ASH);
  params.container = extensions::LAUNCH_CONTAINER_WINDOW;
  OpenApplication(params);
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(id)->status);

  // Find the browser which holds our app.
  Browser* app_browser = NULL;
  const BrowserList* ash_browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  for (BrowserList::const_reverse_iterator it =
           ash_browser_list->begin_last_active();
       it != ash_browser_list->end_last_active() && !app_browser; ++it) {
    if ((*it)->is_app()) {
      app_browser = *it;
      break;
    }
  }
  ASSERT_TRUE(app_browser);

  // After navigating away in the app, we should still be active.
  ui_test_utils::NavigateToURL(app_browser,
                               GURL("http://www.foo.com/bar.html"));
  // Make sure the navigation was entirely performed.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(id)->status);
  app_browser->tab_strip_model()->CloseWebContentsAt(0,
                                                     TabStripModel::CLOSE_NONE);
  // Make sure that the app is really gone.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(ash::STATUS_CLOSED, model_->ItemByID(id)->status);
}

// Checks that a opening a settings window creates a new launcher item.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, SettingsWindow) {
  chrome::SettingsWindowManager* settings_manager =
      chrome::SettingsWindowManager::GetInstance();
  ash::ShelfModel* shelf_model = ash::Shell::GetInstance()->shelf_model();

  // Get the number of items in the shelf and browser menu.
  int item_count = shelf_model->item_count();
  size_t browser_count = NumberOfDetectedLauncherBrowsers(false);

  // Open a settings window. Number of browser items should remain unchanged,
  // number of shelf items should increase.
  settings_manager->ShowChromePageForProfile(
      browser()->profile(),
      chrome::GetSettingsUrl(std::string()));
  Browser* settings_browser =
      settings_manager->FindBrowserForProfile(browser()->profile());
  ASSERT_TRUE(settings_browser);
  EXPECT_EQ(browser_count, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(item_count + 1, shelf_model->item_count());

  // TODO(stevenjb): Test multiprofile on Chrome OS when test support is addded.
  // crbug.com/230464.
}

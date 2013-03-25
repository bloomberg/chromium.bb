// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_util.h"
#include "ash/shell.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/window_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

using extensions::Extension;
using content::WebContents;

namespace {

class TestShellWindowRegistryObserver
    : public extensions::ShellWindowRegistry::Observer {
 public:
  explicit TestShellWindowRegistryObserver(Profile* profile)
      : profile_(profile),
        icon_updates_(0) {
    extensions::ShellWindowRegistry::Get(profile_)->AddObserver(this);
  }

  virtual ~TestShellWindowRegistryObserver() {
    extensions::ShellWindowRegistry::Get(profile_)->RemoveObserver(this);
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

// TODO(skuhne): Change name back to LauncherPlatformAppBrowserTest when the
// old launcher gets ripped out.
class LauncherPlatformPerAppAppBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  LauncherPlatformPerAppAppBrowserTest()
      : launcher_(NULL),
        controller_(NULL) {
  }

  virtual ~LauncherPlatformPerAppAppBrowserTest() {}

  virtual void RunTestOnMainThreadLoop() OVERRIDE {
    launcher_ = ash::Launcher::ForPrimaryDisplay();
    controller_ =
        static_cast<ChromeLauncherControllerPerApp*>(launcher_->delegate());
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
    // the app list, and the item at [count - 2] will be the desired item.
    return launcher_model()->items()[launcher_model()->item_count() - 2];
  }

  const ash::LauncherItem& GetLastLauncherPanelItem() {
    // Panels show up on the right side of the launcher, so the desired item
    // will be the last one.
    return launcher_model()->items()[launcher_model()->item_count() - 1];
  }

  const LauncherItemController* GetItemController(ash::LauncherID id) {
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

  ash::Launcher* launcher_;
  ChromeLauncherControllerPerApp* controller_;

 private:

  DISALLOW_COPY_AND_ASSIGN(LauncherPlatformPerAppAppBrowserTest);
};

// TODO(skuhne): Change name back to LauncherAppBrowserTest when the
// old launcher gets ripped out.
class LauncherPerAppAppBrowserTest : public ExtensionBrowserTest {
 protected:
  LauncherPerAppAppBrowserTest()
      : launcher_(NULL),
        model_(NULL) {
  }

  virtual ~LauncherPerAppAppBrowserTest() {}

  virtual void RunTestOnMainThreadLoop() OVERRIDE {
    launcher_ = ash::Launcher::ForPrimaryDisplay();
    model_ =
        ash::test::ShellTestApi(ash::Shell::GetInstance()).launcher_model();
    return ExtensionBrowserTest::RunTestOnMainThreadLoop();
  }

  size_t NumberOfDetectedLauncherBrowsers(bool show_all_tabs) {
    ChromeLauncherControllerPerApp* controller =
        static_cast<ChromeLauncherControllerPerApp*>(launcher_->delegate());
    int items = controller->GetBrowserApplicationList(
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
    ChromeLauncherController* controller =
        static_cast<ChromeLauncherController*>(launcher_->delegate());
    int item_count = model_->item_count();
    ash::LauncherID shortcut_id = controller->CreateAppShortcutLauncherItem(
        app_id,
        item_count);
    controller->PersistPinnedState();
    EXPECT_EQ(++item_count, model_->item_count());
    const ash::LauncherItem& item = *model_->ItemByID(shortcut_id);
    EXPECT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
    return item.id;
  }

  ash::Launcher* launcher_;
  ash::LauncherModel* model_;

 private:

  DISALLOW_COPY_AND_ASSIGN(LauncherPerAppAppBrowserTest);
};

// TODO(skuhne): Change name to LauncherAppBrowserTestNoBrowser when the
// old launcher gets ripped out.
class LauncherPerAppAppBrowserTestNoDefaultBrowser
    : public LauncherPerAppAppBrowserTest {
 protected:
  LauncherPerAppAppBrowserTestNoDefaultBrowser() {}
  virtual ~LauncherPerAppAppBrowserTestNoDefaultBrowser() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    LauncherPerAppAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(LauncherPerAppAppBrowserTestNoDefaultBrowser);
};

// Test that we can launch a platform app and get a running item.
IN_PROC_BROWSER_TEST_F(LauncherPlatformPerAppAppBrowserTest, LaunchUnpinned) {
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
IN_PROC_BROWSER_TEST_F(LauncherPlatformPerAppAppBrowserTest, LaunchPinned) {
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

IN_PROC_BROWSER_TEST_F(LauncherPlatformPerAppAppBrowserTest, PinRunning) {
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

IN_PROC_BROWSER_TEST_F(LauncherPlatformPerAppAppBrowserTest, UnpinRunning) {
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
IN_PROC_BROWSER_TEST_F(LauncherPlatformPerAppAppBrowserTest, MultipleWindows) {
  int item_count = launcher_model()->item_count();

  // First run app.
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  ShellWindow* window1 = CreateShellWindow(extension);
  ++item_count;
  ASSERT_EQ(item_count, launcher_model()->item_count());
  const ash::LauncherItem& item = GetLastLauncherItem();
  ash::LauncherID item_id = item.id;
  EXPECT_EQ(ash::TYPE_PLATFORM_APP, item.type);
  EXPECT_EQ(ash::STATUS_ACTIVE, item.status);
  EXPECT_EQ(2, GetNumApplicationMenuItems(item));  // Title + 1 window

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

IN_PROC_BROWSER_TEST_F(LauncherPlatformPerAppAppBrowserTest, MultipleApps) {
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
IN_PROC_BROWSER_TEST_F(LauncherPlatformPerAppAppBrowserTest, WindowActivation) {
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
  launcher_->ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id1));
  EXPECT_EQ(ash::STATUS_ACTIVE, launcher_model()->ItemByID(item_id1)->status);
  EXPECT_EQ(ash::STATUS_RUNNING,
            launcher_model()->ItemByID(item_id2)->status);
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));

  // Activate second one.
  launcher_->ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id2));
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
  launcher_->ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id1));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));
  launcher_->ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id1));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));

  // Activate the second app again
  launcher_->ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id2));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));

  // Activate the first app again
  launcher_->ActivateLauncherItem(launcher_model()->ItemIndexByID(item_id1));
  EXPECT_TRUE(ash::wm::IsActiveWindow(window1->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window2->GetNativeWindow()));
  EXPECT_FALSE(ash::wm::IsActiveWindow(window1b->GetNativeWindow()));

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

IN_PROC_BROWSER_TEST_F(LauncherPlatformPerAppAppBrowserTest,
    BrowserActivation) {
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
IN_PROC_BROWSER_TEST_F(LauncherPlatformPerAppAppBrowserTest, SetIcon) {
  TestShellWindowRegistryObserver test_observer(browser()->profile());

  // Enable experimental APIs to allow panel creation.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

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
  // The Panel will be the last item, the app list second-to-last, the app
  // third from last.
  const ash::LauncherItem& app_item =
      launcher_model()->items()[launcher_item_count - 3];
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
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, LaunchPinned) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
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
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, LaunchUnpinned) {
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
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, LaunchInBackground) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  LoadAndLaunchExtension("app1", extension_misc::LAUNCH_TAB,
                         NEW_BACKGROUND_TAB);
  EXPECT_EQ(++tab_count, tab_strip->count());
  ChromeLauncherController::instance()->LaunchApp(last_loaded_extension_id_, 0);
}

// Confirm that clicking a icon for an app running in one of 2 maxmized windows
// activates the right window.
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, LaunchMaximized) {
  aura::Window* window1 = browser()->window()->GetNativeWindow();
  ash::wm::MaximizeWindow(window1);
  content::WindowedNotificationObserver open_observer(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::NotificationService::AllSources());
  chrome::NewEmptyWindow(browser()->profile(), chrome::HOST_DESKTOP_TYPE_ASH);
  open_observer.Wait();
  Browser* browser2 = content::Source<Browser>(open_observer.source()).ptr();
  aura::Window* window2 = browser2->window()->GetNativeWindow();
  TabStripModel* tab_strip = browser2->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::wm::MaximizeWindow(window2);

  ash::LauncherID shortcut_id = CreateShortcut("app1");
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);

  window1->Show();
  ash::wm::ActivateWindow(window1);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut_id)).status);

  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut_id)).status);
}

// Activating the same app multiple times should launch only a single copy.
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, ActivateApp) {
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
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, LaunchApp) {
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
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, MultipleApps) {
  int item_count = model_->item_count();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::LauncherID shortcut1 = CreateShortcut("app1");
  EXPECT_EQ(++item_count, model_->item_count());
  ash::LauncherID shortcut2 = CreateShortcut("app2");
  EXPECT_EQ(++item_count, model_->item_count());

  // Launch first app.
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut1));
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* tab1 = tab_strip->GetActiveWebContents();
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut1)).status);

  // Launch second app.
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut2));
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* tab2 = tab_strip->GetActiveWebContents();
  ASSERT_NE(tab1, tab2);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut2)).status);

  // Reactivate first app.
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut1));
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
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut1));
  EXPECT_EQ(tab_count, tab_strip->count());
  EXPECT_EQ(tab_strip->GetActiveWebContents(), tab1);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut2)).status);

  // And second again. This time the second tab should become active.
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut2));
  EXPECT_EQ(tab_count, tab_strip->count());
  EXPECT_EQ(tab_strip->GetActiveWebContents(), tab2);
  EXPECT_EQ(ash::STATUS_RUNNING, (*model_->ItemByID(shortcut1)).status);
  EXPECT_EQ(ash::STATUS_ACTIVE, (*model_->ItemByID(shortcut2)).status);
}

// Confirm that a page can be navigated from and to while maintaining the
// correct running state.
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, Navigation) {
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_CLOSED, (*model_->ItemByID(shortcut_id)).status);
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
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

IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, MultipleOwnedTabs) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
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
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(tab_strip->GetActiveWebContents(), first_tab);
}

IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, RefocusFilter) {
  ChromeLauncherController* controller =
      static_cast<ChromeLauncherController*>(launcher_->delegate());
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  WebContents* first_tab = tab_strip->GetActiveWebContents();

  controller->SetRefocusURLPatternForTest(
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
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  EXPECT_EQ(tab_strip->GetActiveWebContents(), first_tab);
}

IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, RefocusFilterLaunch) {
  ChromeLauncherController* controller =
      static_cast<ChromeLauncherController*>(launcher_->delegate());
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::LauncherID shortcut_id = CreateShortcut("app1");
  controller->SetRefocusURLPatternForTest(
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
  launcher_->ActivateLauncherItem(model_->ItemIndexByID(shortcut_id));
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* second_tab = tab_strip->GetActiveWebContents();
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->ItemByID(shortcut_id)->status);
  EXPECT_NE(first_tab, second_tab);
  EXPECT_EQ(tab_strip->GetActiveWebContents(), second_tab);
}

// Check the launcher activation state for applications and browser.
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest, ActivationStateCheck) {
  ChromeLauncherController* controller =
      static_cast<ChromeLauncherController*>(launcher_->delegate());
  TabStripModel* tab_strip = browser()->tab_strip_model();
  // Get the browser item index
  int browser_index = ash::launcher::GetBrowserItemIndex(*controller->model());
  EXPECT_TRUE(browser_index >= 0);

  // Even though we are just comming up, the browser should be active.
  EXPECT_EQ(ash::STATUS_ACTIVE, model_->items()[browser_index].status);

  ash::LauncherID shortcut_id = CreateShortcut("app1");
  controller->SetRefocusURLPatternForTest(
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
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTest,
                       AsyncActivationStateCheck) {
  ChromeLauncherController* controller =
      static_cast<ChromeLauncherController*>(launcher_->delegate());
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ash::LauncherID shortcut_id = CreateShortcut("app1");
  controller->SetRefocusURLPatternForTest(
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
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTestNoDefaultBrowser,
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
IN_PROC_BROWSER_TEST_F(LauncherPerAppAppBrowserTestNoDefaultBrowser,
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

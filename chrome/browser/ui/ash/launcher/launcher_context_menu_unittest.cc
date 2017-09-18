// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include <memory>

#include "ash/public/cpp/shelf_item.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/extension_launcher_context_menu.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/exo/shell_surface.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/views/widget/widget.h"

namespace {

// A shell delegate that owns a ChromeLauncherController, like production.
class ChromeLauncherTestShellDelegate : public ash::TestShellDelegate {
 public:
  explicit ChromeLauncherTestShellDelegate(Profile* profile)
      : profile_(profile) {}

  ChromeLauncherController* controller() { return controller_.get(); }

  // ash::TestShellDelegate:
  void ShelfInit() override {
    if (!controller_) {
      controller_ = base::MakeUnique<ChromeLauncherController>(
          profile_, ash::Shell::Get()->shelf_model());
      controller_->Init();
    }
  }
  void ShelfShutdown() override { controller_.reset(); }

 private:
  Profile* profile_;
  std::unique_ptr<ChromeLauncherController> controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherTestShellDelegate);
};

class LauncherContextMenuTest : public ash::AshTestBase {
 protected:
  static bool IsItemPresentInMenu(LauncherContextMenu* menu, int command_id) {
    return menu->GetIndexOfCommandId(command_id) != -1;
  }

  LauncherContextMenuTest() {}

  void SetUp() override {
    arc_test_.SetUp(&profile_);
    session_manager_ = base::MakeUnique<session_manager::SessionManager>();
    shell_delegate_ = new ChromeLauncherTestShellDelegate(&profile_);
    ash_test_helper()->set_test_shell_delegate(shell_delegate_);
    ash::AshTestBase::SetUp();
  }

  std::unique_ptr<LauncherContextMenu> CreateLauncherContextMenu(
      ash::ShelfItemType shelf_item_type,
      int64_t display_id) {
    ash::ShelfItem item;
    item.id = ash::ShelfID("idmockidmockidmockidmockidmockid");
    item.type = shelf_item_type;
    return LauncherContextMenu::Create(controller(), &item, display_id);
  }

  // Creates app window and set optional ARC application id.
  views::Widget* CreateArcWindow(std::string& window_app_id) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    views::Widget* widget = new views::Widget();
    widget->Init(params);
    widget->Show();
    widget->Activate();
    exo::ShellSurface::SetApplicationId(widget->GetNativeWindow(),
                                        window_app_id);
    return widget;
  }

  ArcAppTest& arc_test() { return arc_test_; }

  Profile* profile() { return &profile_; }

  ChromeLauncherController* controller() {
    return shell_delegate_->controller();
  }

 private:
  TestingProfile profile_;
  ChromeLauncherTestShellDelegate* shell_delegate_ = nullptr;
  ArcAppTest arc_test_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;

  DISALLOW_COPY_AND_ASSIGN(LauncherContextMenuTest);
};

// Verifies that "New Incognito window" menu item in the launcher context
// menu is disabled when Incognito mode is switched off (by a policy).
TEST_F(LauncherContextMenuTest,
       NewIncognitoWindowMenuIsDisabledWhenIncognitoModeOff) {
  int64_t primary_id = GetPrimaryDisplay().id();
  // Initially, "New Incognito window" should be enabled.
  std::unique_ptr<LauncherContextMenu> menu =
      CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT, primary_id);
  ASSERT_TRUE(IsItemPresentInMenu(
      menu.get(), LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(menu->IsCommandIdEnabled(
      LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW));

  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  menu = CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT, primary_id);
  // The item should be disabled.
  ASSERT_TRUE(IsItemPresentInMenu(
      menu.get(), LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW));
  EXPECT_FALSE(menu->IsCommandIdEnabled(
      LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW));
}

// Verifies that "New window" menu item in the launcher context
// menu is disabled when Incognito mode is forced (by a policy).
TEST_F(LauncherContextMenuTest,
       NewWindowMenuIsDisabledWhenIncognitoModeForced) {
  int64_t primary_id = GetPrimaryDisplay().id();
  // Initially, "New window" should be enabled.
  std::unique_ptr<LauncherContextMenu> menu =
      CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT, primary_id);
  ASSERT_TRUE(IsItemPresentInMenu(
      menu.get(), LauncherContextMenu::MENU_NEW_WINDOW));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_NEW_WINDOW));

  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  menu = CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT, primary_id);
  ASSERT_TRUE(IsItemPresentInMenu(
      menu.get(), LauncherContextMenu::MENU_NEW_WINDOW));
  EXPECT_FALSE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_NEW_WINDOW));
}

// Verifies that "Close" is not shown in context menu if no browser window is
// opened.
TEST_F(LauncherContextMenuTest,
       DesktopShellLauncherContextMenuVerifyCloseItem) {
  int64_t primary_id = GetPrimaryDisplay().id();
  std::unique_ptr<LauncherContextMenu> menu =
      CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT, primary_id);
  ASSERT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));
}

// Verifies contextmenu items for ARC app
TEST_F(LauncherContextMenuTest, ArcLauncherContextMenuItemCheck) {
  arc_test().app_instance()->RefreshAppList();
  arc_test().app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfo>(arc_test().fake_apps().begin(),
                                       arc_test().fake_apps().begin() + 1));
  const std::string app_id = ArcAppTest::GetAppId(arc_test().fake_apps()[0]);

  controller()->PinAppWithID(app_id);

  const ash::ShelfItem* item = controller()->GetItem(ash::ShelfID(app_id));
  ASSERT_TRUE(item);
  int64_t primary_id = GetPrimaryDisplay().id();
  std::unique_ptr<LauncherContextMenu> menu =
      base::MakeUnique<ArcLauncherContextMenu>(controller(), item, primary_id);

  // ARC app is pinned but not running.
  EXPECT_TRUE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_PIN));
  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));

  // ARC app is running.
  std::string window_app_id1("org.chromium.arc.1");
  CreateArcWindow(window_app_id1);
  arc_test().app_instance()->SendTaskCreated(1, arc_test().fake_apps()[0],
                                             std::string());
  menu =
      base::MakeUnique<ArcLauncherContextMenu>(controller(), item, primary_id);

  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_CLOSE));

  // ARC non-launchable app is running.
  const std::string app_id2 = ArcAppTest::GetAppId(arc_test().fake_apps()[1]);
  std::string window_app_id2("org.chromium.arc.2");
  CreateArcWindow(window_app_id2);
  arc_test().app_instance()->SendTaskCreated(2, arc_test().fake_apps()[1],
                                             std::string());
  const ash::ShelfItem* item2 = controller()->GetItem(ash::ShelfID(app_id2));
  ASSERT_TRUE(item2);
  menu =
      base::MakeUnique<ArcLauncherContextMenu>(controller(), item2, primary_id);

  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_CLOSE));

  // Shelf group context menu.
  std::vector<arc::mojom::ShortcutInfo> shortcuts = arc_test().fake_shortcuts();
  shortcuts[0].intent_uri +=
      ";S.org.chromium.arc.shelf_group_id=arc_test_shelf_group;end";
  arc_test().app_instance()->SendInstallShortcuts(shortcuts);
  const std::string app_id3 =
      arc::ArcAppShelfId("arc_test_shelf_group",
                         ArcAppTest::GetAppId(arc_test().fake_apps()[2]))
          .ToString();
  std::string window_app_id3("org.chromium.arc.3");
  CreateArcWindow(window_app_id3);
  arc_test().app_instance()->SendTaskCreated(3, arc_test().fake_apps()[2],
                                             shortcuts[0].intent_uri);
  const ash::ShelfItem* item3 = controller()->GetItem(ash::ShelfID(app_id3));
  ASSERT_TRUE(item3);

  menu =
      base::MakeUnique<ArcLauncherContextMenu>(controller(), item3, primary_id);

  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_CLOSE));
}

}  // namespace

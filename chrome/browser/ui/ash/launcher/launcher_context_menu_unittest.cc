// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include "ash/common/shelf/shelf_item_types.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_impl.h"
#include "chrome/browser/ui/ash/launcher/desktop_shell_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/extension_launcher_context_menu.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/exo/shell_surface.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/widget/widget.h"

class LauncherContextMenuTest : public ash::test::AshTestBase {
 protected:
  static bool IsItemPresentInMenu(LauncherContextMenu* menu, int command_id) {
    return menu->GetIndexOfCommandId(command_id) != -1;
  }

  LauncherContextMenuTest() : profile_(new TestingProfile()) {}

  void SetUp() override {
    arc_test_.SetUp(profile_.get());
    ash::test::AshTestBase::SetUp();
    controller_.reset(new ChromeLauncherControllerImpl(
        profile(), ash::WmShell::Get()->shelf_model()));
    controller_->Init();
  }

  void TearDown() override {
    controller_.reset(nullptr);
    ash::test::AshTestBase::TearDown();
  }

  ash::WmShelf* GetWmShelf() {
    return ash::WmShell::Get()
        ->GetPrimaryRootWindow()
        ->GetRootWindowController()
        ->GetShelf();
  }

  LauncherContextMenu* CreateLauncherContextMenu(
      ash::ShelfItemType shelf_item_type) {
    ash::ShelfItem item;
    item.id = 123;  // dummy id
    item.type = shelf_item_type;
    return LauncherContextMenu::Create(controller_.get(), &item, GetWmShelf());
  }

  LauncherContextMenu* CreateLauncherContextMenuForDesktopShell() {
    ash::ShelfItem* item = nullptr;
    return LauncherContextMenu::Create(controller_.get(), item, GetWmShelf());
  }

  // Creates app window and set optional Arc application id.
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

  Profile* profile() { return profile_.get(); }

  ChromeLauncherControllerImpl* controller() { return controller_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeLauncherControllerImpl> controller_;
  ArcAppTest arc_test_;

  DISALLOW_COPY_AND_ASSIGN(LauncherContextMenuTest);
};

// Verifies that "New Incognito window" menu item in the launcher context
// menu is disabled when Incognito mode is switched off (by a policy).
TEST_F(LauncherContextMenuTest,
       NewIncognitoWindowMenuIsDisabledWhenIncognitoModeOff) {
  // Initially, "New Incognito window" should be enabled.
  std::unique_ptr<LauncherContextMenu> menu(
      CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT));
  ASSERT_TRUE(IsItemPresentInMenu(
      menu.get(), LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(menu->IsCommandIdEnabled(
      LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW));

  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  menu.reset(CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT));
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
  // Initially, "New window" should be enabled.
  std::unique_ptr<LauncherContextMenu> menu(
      CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT));
  ASSERT_TRUE(IsItemPresentInMenu(
      menu.get(), LauncherContextMenu::MENU_NEW_WINDOW));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_NEW_WINDOW));

  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  menu.reset(CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT));
  ASSERT_TRUE(IsItemPresentInMenu(
      menu.get(), LauncherContextMenu::MENU_NEW_WINDOW));
  EXPECT_FALSE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_NEW_WINDOW));
}

// Verifies status of contextmenu items for desktop shell.
TEST_F(LauncherContextMenuTest, DesktopShellLauncherContextMenuItemCheck) {
  std::unique_ptr<LauncherContextMenu> menu(
      CreateLauncherContextMenuForDesktopShell());
  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_AUTO_HIDE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_AUTO_HIDE));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(),
                                  LauncherContextMenu::MENU_ALIGNMENT_MENU));
  EXPECT_TRUE(
      menu->IsCommandIdEnabled(LauncherContextMenu::MENU_ALIGNMENT_MENU));
  // By default, screen is not locked and ChangeWallPaper item is added in
  // menu. ChangeWallPaper item is not enabled in default mode.
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(),
                                  LauncherContextMenu::MENU_CHANGE_WALLPAPER));
  EXPECT_FALSE(
      menu->IsCommandIdEnabled(LauncherContextMenu::MENU_CHANGE_WALLPAPER));
}

// Verifies that "Close" is not shown in context menu if no browser window is
// opened.
TEST_F(LauncherContextMenuTest,
       DesktopShellLauncherContextMenuVerifyCloseItem) {
  std::unique_ptr<LauncherContextMenu> menu(
      CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT));
  ASSERT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));
}

// Verifies contextmenu items for Arc app
TEST_F(LauncherContextMenuTest, ArcLauncherContextMenuItemCheck) {
  arc_test().app_instance()->RefreshAppList();
  arc_test().app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfo>(arc_test().fake_apps().begin(),
                                       arc_test().fake_apps().begin() + 1));
  const std::string app_id = ArcAppTest::GetAppId(arc_test().fake_apps()[0]);

  controller()->PinAppWithID(app_id);

  ash::ShelfItem item;
  item.id = controller()->GetShelfIDForAppID(app_id);
  ash::WmShelf* wm_shelf = GetWmShelf();

  std::unique_ptr<LauncherContextMenu> menu(
      new ArcLauncherContextMenu(controller(), &item, wm_shelf));

  // Arc app is pinned but not running.
  EXPECT_TRUE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_PIN));
  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));

  EXPECT_TRUE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_AUTO_HIDE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_AUTO_HIDE));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(),
                                  LauncherContextMenu::MENU_ALIGNMENT_MENU));
  EXPECT_TRUE(
      menu->IsCommandIdEnabled(LauncherContextMenu::MENU_ALIGNMENT_MENU));
  // By default, screen is not locked and ChangeWallPaper item is added in
  // menu. ChangeWallPaper item is not enabled in default mode.
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(),
                                  LauncherContextMenu::MENU_CHANGE_WALLPAPER));
  EXPECT_FALSE(
      menu->IsCommandIdEnabled(LauncherContextMenu::MENU_CHANGE_WALLPAPER));

  // Arc app is running.
  std::string window_app_id1("org.chromium.arc.1");
  CreateArcWindow(window_app_id1);
  arc_test().app_instance()->SendTaskCreated(1, arc_test().fake_apps()[0],
                                             std::string());
  menu.reset(new ArcLauncherContextMenu(controller(), &item, wm_shelf));

  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_CLOSE));

  // Arc non-launchable app is running.
  const std::string app_id2 = ArcAppTest::GetAppId(arc_test().fake_apps()[1]);
  std::string window_app_id2("org.chromium.arc.2");
  CreateArcWindow(window_app_id2);
  arc_test().app_instance()->SendTaskCreated(2, arc_test().fake_apps()[1],
                                             std::string());
  item.id = controller()->GetShelfIDForAppID(app_id2);
  ASSERT_TRUE(item.id);
  menu.reset(new ArcLauncherContextMenu(controller(), &item, wm_shelf));

  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_CLOSE));
}

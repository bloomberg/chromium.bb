// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_item_types.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/desktop_shell_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/extension_launcher_context_menu.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window_event_dispatcher.h"

class LauncherContextMenuTest : public ash::test::AshTestBase {
 protected:
  static bool IsItemPresentInMenu(LauncherContextMenu* menu, int command_id) {
    return menu->GetIndexOfCommandId(command_id) != -1;
  }

  LauncherContextMenuTest() : profile_(new TestingProfile()) {}

  void SetUp() override {
    ash::test::AshTestBase::SetUp();
    controller_.reset(new ChromeLauncherController(profile(), &shelf_model_));
  }

  void TearDown() override {
    controller_.reset(nullptr);
    ash::test::AshTestBase::TearDown();
  }

  LauncherContextMenu* CreateLauncherContextMenu(
      ash::ShelfItemType shelf_item_type) {
    ash::ShelfItem item;
    item.id = 1;  // dummy id
    item.type = shelf_item_type;
    ash::Shelf* shelf = ash::Shelf::ForWindow(CurrentContext());
    return LauncherContextMenu::Create(controller_.get(), &item, shelf);
  }

  LauncherContextMenu* CreateLauncherContextMenuForDesktopShell() {
    ash::ShelfItem* item = nullptr;
    ash::Shelf* shelf = ash::Shelf::ForWindow(CurrentContext());
    return LauncherContextMenu::Create(controller_.get(), item, shelf);
  }

  LauncherContextMenu* CreateLauncherContextMenuForArcApp() {
    ash::ShelfItem item;
    item.id = 1;  // dummy id
    ash::Shelf* shelf = ash::Shelf::ForWindow(CurrentContext());
    return new ArcLauncherContextMenu(controller_.get(), &item, shelf);
  }

  Profile* profile() { return profile_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  ash::ShelfModel shelf_model_;
  std::unique_ptr<ChromeLauncherController> controller_;

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

// Verifies contextmenu items for Arc app
TEST_F(LauncherContextMenuTest, ArcLauncherContextMenuItemCheck) {
  std::unique_ptr<LauncherContextMenu> menu(
      CreateLauncherContextMenuForArcApp());
  EXPECT_TRUE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_OPEN_NEW));
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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/base/keycodes/keyboard_codes.h"

typedef BrowserWithTestWindowTest BrowserCommandControllerTest;

TEST_F(BrowserCommandControllerTest, IsReservedCommandOrKey) {
#if defined(OS_CHROMEOS)
  // F1-3 keys are reserved Chrome accelerators on Chrome OS.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_BACK, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                                ui::VKEY_F1, 0, 0)));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FORWARD, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                                   ui::VKEY_F2, 0, 0)));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                                  ui::VKEY_F3, 0, 0)));

  // When there are modifier keys pressed, don't reserve.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD_IGNORING_CACHE, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_F3, ui::EF_SHIFT_DOWN, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD_IGNORING_CACHE, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_F3, ui::EF_CONTROL_DOWN, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FULLSCREEN, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_F4, ui::EF_SHIFT_DOWN, 0)));

  // F4-10 keys are not reserved since they are Ash accelerators.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F4, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F5, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F6, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F7, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F8, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F9, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F10, 0, 0)));

  // Shift+Control+Alt+F3 is also an Ash accelerator. Don't reserve it.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false,
          ui::VKEY_F3,
          ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, 0)));
#endif  // OS_CHROMEOS

#if defined(USE_AURA)
  // Ctrl+n, Ctrl+w are reserved while Ctrl+f is not.

  // The content::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_NEW_WINDOW, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_N, ui::EF_CONTROL_DOWN, 0)));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_CLOSE_TAB, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_W, ui::EF_CONTROL_DOWN, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FIND, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_F, ui::EF_CONTROL_DOWN, 0)));
#endif  // USE_AURA
}

TEST_F(BrowserCommandControllerTest, IsReservedCommandOrKeyIsApp) {
  browser()->app_name_ = "app";
  ASSERT_TRUE(browser()->is_app());

  // When is_app(), no keys are reserved.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_BACK, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                                ui::VKEY_F1, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FORWARD, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                                   ui::VKEY_F2, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                                  ui::VKEY_F3, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F4, 0, 0)));
#endif  // OS_CHROMEOS

#if defined(USE_AURA)
  // The content::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_NEW_WINDOW, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_N, ui::EF_CONTROL_DOWN, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_CLOSE_TAB, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_W, ui::EF_CONTROL_DOWN, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FIND, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_F, ui::EF_CONTROL_DOWN, 0)));
#endif  // USE_AURA
}

TEST_F(BrowserCommandControllerTest, AppFullScreen) {
  // Enable for tabbed browser.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Enabled for app windows.
  browser()->app_name_ = "app";
  ASSERT_TRUE(browser()->is_app());
  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Enabled for panels.
  Browser::CreateParams panel_params(Browser::TYPE_PANEL, profile());
  TestBrowserWindow panel_window;
  panel_params.window = &panel_window;
  Browser panel_browser(panel_params);
  ASSERT_TRUE(panel_browser.is_type_panel());
  EXPECT_TRUE(chrome::IsCommandEnabled(&panel_browser, IDC_FULLSCREEN));

  // Disabled for app-panels.
  panel_browser.app_name_ = "app";
  ASSERT_TRUE(panel_browser.is_app());
  panel_browser.command_controller()->FullscreenStateChanged();
  EXPECT_FALSE(chrome::IsCommandEnabled(&panel_browser, IDC_FULLSCREEN));
}

TEST_F(BrowserCommandControllerTest, AvatarMenuDisabledWhenOnlyOneProfile) {
  if (!ProfileManager::IsMultipleProfilesEnabled())
    return;

  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  ProfileManager* profile_manager = testing_profile_manager.profile_manager();

  chrome::BrowserCommandController command_controller(browser(),
                                                      profile_manager);
  const CommandUpdater* command_updater = command_controller.command_updater();

  testing_profile_manager.CreateTestingProfile("p1");
  ASSERT_EQ(1U, profile_manager->GetNumberOfProfiles());
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager.CreateTestingProfile("p2");
  ASSERT_EQ(2U, profile_manager->GetNumberOfProfiles());
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager.DeleteTestingProfile("p1");
  ASSERT_EQ(1U, profile_manager->GetNumberOfProfiles());
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager.DeleteTestingProfile("p2");
}

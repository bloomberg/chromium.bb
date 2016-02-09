// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

typedef BrowserWithTestWindowTest BrowserCommandControllerTest;

TEST_F(BrowserCommandControllerTest, IsReservedCommandOrKey) {
#if defined(OS_CHROMEOS)
  // F1-3 keys are reserved Chrome accelerators on Chrome OS.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_BACK, content::NativeWebKeyboardEvent(
                    ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_BACK,
                                 ui::DomCode::BROWSER_BACK, 0))));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FORWARD, content::NativeWebKeyboardEvent(ui::KeyEvent(
                       ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_FORWARD,
                       ui::DomCode::BROWSER_FORWARD, 0))));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD, content::NativeWebKeyboardEvent(
                      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_REFRESH,
                                   ui::DomCode::BROWSER_REFRESH, 0))));

  // When there are modifier keys pressed, don't reserve.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD_IGNORING_CACHE, content::NativeWebKeyboardEvent(ui::KeyEvent(
                                     ui::ET_KEY_PRESSED, ui::VKEY_F3,
                                     ui::DomCode::F3, ui::EF_SHIFT_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD_IGNORING_CACHE, content::NativeWebKeyboardEvent(ui::KeyEvent(
                                     ui::ET_KEY_PRESSED, ui::VKEY_F3,
                                     ui::DomCode::F3, ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FULLSCREEN, content::NativeWebKeyboardEvent(
                          ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_F4,
                                       ui::DomCode::F4, ui::EF_SHIFT_DOWN))));

  // F4-10 keys are not reserved since they are Ash accelerators.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F4, ui::DomCode::F4, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F5, ui::DomCode::F5, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F6, ui::DomCode::F6, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F7, ui::DomCode::F7, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F8, ui::DomCode::F8, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F9, ui::DomCode::F9, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F10, ui::DomCode::F10, 0))));

  // Shift+Control+Alt+F3 is also an Ash accelerator. Don't reserve it.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F3, ui::DomCode::F3,
              ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN))));
#endif  // OS_CHROMEOS

#if defined(USE_AURA)
  // Ctrl+n, Ctrl+w are reserved while Ctrl+f is not.

  // The content::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_NEW_WINDOW, content::NativeWebKeyboardEvent(ui::KeyEvent(
                          ui::ET_KEY_PRESSED, ui::VKEY_N, ui::DomCode::US_N,
                          ui::EF_CONTROL_DOWN))));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_CLOSE_TAB, content::NativeWebKeyboardEvent(ui::KeyEvent(
                         ui::ET_KEY_PRESSED, ui::VKEY_W, ui::DomCode::US_W,
                         ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FIND, content::NativeWebKeyboardEvent(
                    ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_F,
                                 ui::DomCode::US_F, ui::EF_CONTROL_DOWN))));
#endif  // USE_AURA
}

TEST_F(BrowserCommandControllerTest, IsReservedCommandOrKeyIsApp) {
  browser()->app_name_ = "app";
  ASSERT_TRUE(browser()->is_app());

  // When is_app(), no keys are reserved.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_BACK, content::NativeWebKeyboardEvent(ui::KeyEvent(
                    ui::ET_KEY_PRESSED, ui::VKEY_F1, ui::DomCode::F1, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FORWARD, content::NativeWebKeyboardEvent(ui::KeyEvent(
                       ui::ET_KEY_PRESSED, ui::VKEY_F2, ui::DomCode::F2, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD, content::NativeWebKeyboardEvent(ui::KeyEvent(
                      ui::ET_KEY_PRESSED, ui::VKEY_F3, ui::DomCode::F3, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F4, ui::DomCode::F4, 0))));
#endif  // OS_CHROMEOS

#if defined(USE_AURA)
  // The content::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_NEW_WINDOW, content::NativeWebKeyboardEvent(ui::KeyEvent(
                          ui::ET_KEY_PRESSED, ui::VKEY_N, ui::DomCode::US_N,
                          ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_CLOSE_TAB, content::NativeWebKeyboardEvent(ui::KeyEvent(
                         ui::ET_KEY_PRESSED, ui::VKEY_W, ui::DomCode::US_W,
                         ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FIND, content::NativeWebKeyboardEvent(
                    ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_F,
                                 ui::DomCode::US_F, ui::EF_CONTROL_DOWN))));
#endif  // USE_AURA
}

TEST_F(BrowserCommandControllerTest, IncognitoCommands) {
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_SHOW_SIGNIN));

  TestingProfile* testprofile = browser()->profile()->AsTestingProfile();
  EXPECT_TRUE(testprofile);
  testprofile->SetGuestSession(true);
  chrome::BrowserCommandController
    ::UpdateSharedCommandsForIncognitoAvailability(
      browser()->command_controller()->command_updater(), testprofile);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_SIGNIN));

  testprofile->SetGuestSession(false);
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  chrome::BrowserCommandController
    ::UpdateSharedCommandsForIncognitoAvailability(
      browser()->command_controller()->command_updater(), testprofile);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_SIGNIN));
}

TEST_F(BrowserCommandControllerTest, AppFullScreen) {
  // Enable for tabbed browser.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Enabled for app windows.
  browser()->app_name_ = "app";
  ASSERT_TRUE(browser()->is_app());
  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));
}

TEST_F(BrowserCommandControllerTest, AvatarAcceleratorEnabledOnDesktop) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  ProfileManager* profile_manager = testing_profile_manager.profile_manager();

  chrome::BrowserCommandController command_controller(browser());
  const CommandUpdater* command_updater = command_controller.command_updater();

  bool enabled = true;
#if defined(OS_CHROMEOS)
  // Chrome OS uses system tray menu to handle multi-profiles.
  enabled = false;
#endif

  testing_profile_manager.CreateTestingProfile("p1");
  ASSERT_EQ(1U, profile_manager->GetNumberOfProfiles());
  EXPECT_EQ(enabled, command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager.CreateTestingProfile("p2");
  ASSERT_EQ(2U, profile_manager->GetNumberOfProfiles());
  EXPECT_EQ(enabled, command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager.DeleteTestingProfile("p1");
  ASSERT_EQ(1U, profile_manager->GetNumberOfProfiles());
  EXPECT_EQ(enabled, command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager.DeleteTestingProfile("p2");
}

TEST_F(BrowserCommandControllerTest, AvatarMenuAlwaysDisabledInIncognitoMode) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // Set up a profile with an off the record profile.
  TestingProfile::Builder normal_builder;
  scoped_ptr<TestingProfile> original_profile = normal_builder.Build();

  // Create a new browser based on the off the record profile.
  Browser::CreateParams profile_params(
      original_profile->GetOffTheRecordProfile());
  scoped_ptr<Browser> otr_browser(
      chrome::CreateBrowserWithTestWindowForParams(&profile_params));

  chrome::BrowserCommandController command_controller(otr_browser.get());
  const CommandUpdater* command_updater = command_controller.command_updater();

  // The avatar menu should be disabled.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
  // The command line is reset at the end of every test by the test suite.
}

//////////////////////////////////////////////////////////////////////////////
class BrowserCommandControllerFullscreenTest;

// A test browser window that can toggle fullscreen state.
class FullscreenTestBrowserWindow : public TestBrowserWindow,
                                    ExclusiveAccessContext {
 public:
  FullscreenTestBrowserWindow(
      BrowserCommandControllerFullscreenTest* test_browser)
      : fullscreen_(false), test_browser_(test_browser) {}

  ~FullscreenTestBrowserWindow() override {}

  // TestBrowserWindow overrides:
  bool ShouldHideUIForFullscreen() const override { return fullscreen_; }
  bool IsFullscreen() const override { return fullscreen_; }
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType type,
                       bool with_toolbar) override {
    fullscreen_ = true;
  }
  void ExitFullscreen() override { fullscreen_ = false; }

  ExclusiveAccessContext* GetExclusiveAccessContext() override { return this; }

  // Exclusive access interface:
  Profile* GetProfile() override;
  content::WebContents* GetActiveWebContents() override;
  void HideDownloadShelf() override {}
  void UnhideDownloadShelf() override {}
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type) override {}
  void OnExclusiveAccessUserInput() override {}
  bool IsFullscreenWithToolbar() const override { return IsFullscreen(); }

 private:
  bool fullscreen_;
  BrowserCommandControllerFullscreenTest* test_browser_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenTestBrowserWindow);
};

// Test that uses FullscreenTestBrowserWindow for its window.
class BrowserCommandControllerFullscreenTest
    : public BrowserWithTestWindowTest {
 public:
  BrowserCommandControllerFullscreenTest() {}
  ~BrowserCommandControllerFullscreenTest() override {}

  Browser* GetBrowser() { return BrowserWithTestWindowTest::browser(); }

  // BrowserWithTestWindowTest overrides:
  BrowserWindow* CreateBrowserWindow() override {
    return new FullscreenTestBrowserWindow(this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCommandControllerFullscreenTest);
};

Profile* FullscreenTestBrowserWindow::GetProfile() {
  return test_browser_->GetBrowser()->profile();
}

content::WebContents* FullscreenTestBrowserWindow::GetActiveWebContents() {
  return test_browser_->GetBrowser()->tab_strip_model()->GetActiveWebContents();
}

TEST_F(BrowserCommandControllerFullscreenTest,
       UpdateCommandsForFullscreenMode) {
  // Defaults for a tabbed browser.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_AS_TAB));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_TOOLBAR));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_SEARCH));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_MENU_BAR));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_NEXT_PANE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_PREVIOUS_PANE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_BOOKMARKS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEVELOPER_MENU));
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FEEDBACK));
#endif
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_EDIT_SEARCH_ENGINES));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_VIEW_PASSWORDS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_SHOW_APP_MENU));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Simulate going fullscreen.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  browser()->command_controller()->FullscreenStateChanged();

  // Most commands are disabled in fullscreen.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_AS_TAB));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_TOOLBAR));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_SEARCH));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_MENU_BAR));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_NEXT_PANE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_PREVIOUS_PANE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_BOOKMARKS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_DEVELOPER_MENU));
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FEEDBACK));
#endif
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_EDIT_SEARCH_ENGINES));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_VIEW_PASSWORDS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_APP_MENU));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Exit fullscreen.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_AS_TAB));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_TOOLBAR));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_SEARCH));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_MENU_BAR));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_NEXT_PANE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_PREVIOUS_PANE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_BOOKMARKS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEVELOPER_MENU));
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FEEDBACK));
#endif
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_EDIT_SEARCH_ENGINES));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_VIEW_PASSWORDS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_SHOW_APP_MENU));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Guest Profiles disallow some options.
  TestingProfile* testprofile = browser()->profile()->AsTestingProfile();
  EXPECT_TRUE(testprofile);
  testprofile->SetGuestSession(true);

  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
}

TEST_F(BrowserCommandControllerTest, IncognitoModeOnSigninAllowedPrefChange) {
  // Set up a profile with an off the record profile.
  scoped_ptr<TestingProfile> profile1 = TestingProfile::Builder().Build();
  Profile* profile2 = profile1->GetOffTheRecordProfile();

  EXPECT_EQ(profile2->GetOriginalProfile(), profile1.get());

  // Create a new browser based on the off the record profile.
  Browser::CreateParams profile_params(profile1->GetOffTheRecordProfile());
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&profile_params));

  chrome::BrowserCommandController command_controller(browser2.get());
  const CommandUpdater* command_updater = command_controller.command_updater();

  // Check that the SYNC_SETUP command is updated on preference change.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_SYNC_SETUP));
  profile1->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_SYNC_SETUP));
}

TEST_F(BrowserCommandControllerTest, OnSigninAllowedPrefChange) {
  chrome::BrowserCommandController command_controller(browser());
  const CommandUpdater* command_updater = command_controller.command_updater();

  // Check that the SYNC_SETUP command is updated on preference change.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_SYNC_SETUP));
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_SYNC_SETUP));
}

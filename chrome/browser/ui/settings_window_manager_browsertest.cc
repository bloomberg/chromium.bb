// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/settings_window_manager.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

namespace {

class SettingsWindowTestObserver
    : public chrome::SettingsWindowManagerObserver {
 public:
  SettingsWindowTestObserver() : browser_(NULL), new_settings_count_(0) {}
  virtual ~SettingsWindowTestObserver() {}

  virtual void OnNewSettingsWindow(Browser* settings_browser) OVERRIDE {
    browser_ = settings_browser;
    ++new_settings_count_;
  }

  Browser* browser() { return browser_; }
  size_t new_settings_count() const { return new_settings_count_; }

 private:
  Browser* browser_;
  size_t new_settings_count_;

  DISALLOW_COPY_AND_ASSIGN(SettingsWindowTestObserver);
};

}  // namespace

class SettingsWindowManagerTest : public InProcessBrowserTest {
 public:
  SettingsWindowManagerTest() : test_profile_(NULL) {
    chrome::SettingsWindowManager::GetInstance()->AddObserver(&observer_);
  }
  virtual ~SettingsWindowManagerTest() {
    chrome::SettingsWindowManager::GetInstance()->RemoveObserver(&observer_);
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(::switches::kMultiProfiles);
  }

  Profile* CreateTestProfile() {
    CHECK(!test_profile_);

    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        profile_manager->GenerateNextProfileDirectoryPath(),
        base::Bind(&SettingsWindowManagerTest::ProfileInitialized,
                   base::Unretained(this),
                   run_loop.QuitClosure()),
        base::string16(),
        base::string16(),
        std::string());
    run_loop.Run();

    return test_profile_;
  }

  void ProfileInitialized(const base::Closure& closure,
                          Profile* profile,
                          Profile::CreateStatus status) {
    if (status == Profile::CREATE_STATUS_INITIALIZED) {
      test_profile_ = profile;
      closure.Run();
    }
  }

  void CloseBrowserSynchronously(Browser* browser) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::NotificationService::AllSources());
    browser->window()->Close();
    observer.Wait();
  }

 protected:
  SettingsWindowTestObserver observer_;
  base::ScopedTempDir temp_profile_dir_;
  Profile* test_profile_;  // Owned by g_browser_process->profile_manager()

  DISALLOW_COPY_AND_ASSIGN(SettingsWindowManagerTest);
};


IN_PROC_BROWSER_TEST_F(SettingsWindowManagerTest, OpenSettingsWindow) {
  chrome::SettingsWindowManager* settings_manager =
      chrome::SettingsWindowManager::GetInstance();

  // Open a settings window.
  settings_manager->ShowForProfile(browser()->profile(), std::string());
  Browser* settings_browser =
      settings_manager->FindBrowserForProfile(browser()->profile());
  ASSERT_TRUE(settings_browser);
  // Ensure the observer fired correctly.
  EXPECT_EQ(1u, observer_.new_settings_count());
  EXPECT_EQ(settings_browser, observer_.browser());

  // Open the settings again: no new window.
  settings_manager->ShowForProfile(browser()->profile(), std::string());
  EXPECT_EQ(settings_browser,
            settings_manager->FindBrowserForProfile(browser()->profile()));
  EXPECT_EQ(1u, observer_.new_settings_count());

  // Close the settings window.
  CloseBrowserSynchronously(settings_browser);
  EXPECT_FALSE(settings_manager->FindBrowserForProfile(browser()->profile()));

  // Open a new settings window.
  settings_manager->ShowForProfile(browser()->profile(), std::string());
  Browser* settings_browser2 =
      settings_manager->FindBrowserForProfile(browser()->profile());
  ASSERT_TRUE(settings_browser2);
  EXPECT_EQ(2u, observer_.new_settings_count());

  CloseBrowserSynchronously(settings_browser2);
}

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SettingsWindowManagerTest, SettingsWindowMultiProfile) {
  chrome::SettingsWindowManager* settings_manager =
      chrome::SettingsWindowManager::GetInstance();
  Profile* test_profile = CreateTestProfile();
  ASSERT_TRUE(test_profile);

  // Open a settings window.
  settings_manager->ShowForProfile(browser()->profile(), std::string());
  Browser* settings_browser =
      settings_manager->FindBrowserForProfile(browser()->profile());
  ASSERT_TRUE(settings_browser);
  // Ensure the observer fired correctly.
  EXPECT_EQ(1u, observer_.new_settings_count());
  EXPECT_EQ(settings_browser, observer_.browser());

  // Open a settings window for a new profile.
  settings_manager->ShowForProfile(test_profile, std::string());
  Browser* settings_browser2 =
      settings_manager->FindBrowserForProfile(test_profile);
  ASSERT_TRUE(settings_browser2);
  // Ensure the observer fired correctly.
  EXPECT_EQ(2u, observer_.new_settings_count());
  EXPECT_EQ(settings_browser2, observer_.browser());

  CloseBrowserSynchronously(settings_browser);
  CloseBrowserSynchronously(settings_browser2);
}
#endif

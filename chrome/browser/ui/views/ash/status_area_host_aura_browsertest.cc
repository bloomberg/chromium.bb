// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/views/ash/status_area_host_aura.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "ui/gfx/size.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

typedef InProcessBrowserTest StatusAreaHostAuraTest;

IN_PROC_BROWSER_TEST_F(StatusAreaHostAuraTest, TextStyle) {
  ChromeShellDelegate* delegate = static_cast<ChromeShellDelegate*>(
      ash::Shell::GetInstance()->delegate());
  StatusAreaHostAura* host = delegate->status_area_host();

#if defined(OS_CHROMEOS)
  ASSERT_FALSE(chromeos::UserManager::Get()->IsUserLoggedIn());
  if (base::chromeos::IsRunningOnChromeOS()) {
    EXPECT_EQ(StatusAreaButton::GRAY_PLAIN_LIGHT,
              host->GetStatusAreaTextStyle());
  } else {
    EXPECT_EQ(StatusAreaButton::WHITE_HALOED_BOLD,
              host->GetStatusAreaTextStyle());
  }

  // ProfileManager expects a profile dir to be set on Chrome OS.
  CommandLine::ForCurrentProcess()->AppendSwitchNative(
      switches::kLoginProfile, "StatusAreaHostAuraTest");
  chromeos::UserManager::Get()->UserLoggedIn("foo@example.com");
  ASSERT_TRUE(chromeos::UserManager::Get()->IsUserLoggedIn());
#endif

  Browser* browser = CreateBrowser(ProfileManager::GetDefaultProfile());
  EXPECT_EQ(StatusAreaButton::WHITE_HALOED_BOLD,
            host->GetStatusAreaTextStyle());

#if defined(OS_CHROMEOS)
  // Lock the screen.
  chromeos::ScreenLocker::Show();
  scoped_ptr<chromeos::test::ScreenLockerTester> tester(
      chromeos::ScreenLocker::GetTester());
  tester->EmulateWindowManagerReady();
  ui_test_utils::WindowedNotificationObserver lock_state_observer(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::NotificationService::AllSources());
  if (!tester->IsLocked())
    lock_state_observer.Wait();
  ASSERT_TRUE(tester->IsLocked());
  EXPECT_EQ(StatusAreaButton::GRAY_PLAIN_LIGHT, host->GetStatusAreaTextStyle());

  chromeos::ScreenLocker::Hide();
  ui_test_utils::RunAllPendingInMessageLoop();
  ASSERT_FALSE(tester->IsLocked());

  EXPECT_EQ(StatusAreaButton::WHITE_HALOED_BOLD,
            host->GetStatusAreaTextStyle());
#endif

  browser->CloseWindow();
}

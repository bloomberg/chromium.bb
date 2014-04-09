// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_bubble_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "grit/generated_resources.h"

class AvatarMenuButtonTest : public InProcessBrowserTest {
 public:
  AvatarMenuButtonTest();
  virtual ~AvatarMenuButtonTest();

 protected:
  void CreateTestingProfile();
  AvatarMenuButton* GetAvatarMenuButton();
  void StartAvatarMenu();

 private:
  DISALLOW_COPY_AND_ASSIGN(AvatarMenuButtonTest);
};

AvatarMenuButtonTest::AvatarMenuButtonTest() {
}

AvatarMenuButtonTest::~AvatarMenuButtonTest() {
}

void AvatarMenuButtonTest::CreateTestingProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(1u, profile_manager->GetNumberOfProfiles());

  base::FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.AppendASCII("test_profile");
  if (!base::PathExists(path))
    ASSERT_TRUE(base::CreateDirectory(path));
  Profile* profile =
      Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
  profile_manager->RegisterTestingProfile(profile, true, false);
  EXPECT_EQ(2u, profile_manager->GetNumberOfProfiles());
}

AvatarMenuButton* AvatarMenuButtonTest::GetAvatarMenuButton() {
  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  return browser_view->frame()->GetAvatarMenuButton();
}

void AvatarMenuButtonTest::StartAvatarMenu() {
  AvatarMenuButton* button = GetAvatarMenuButton();
  ASSERT_TRUE(button);

  AvatarMenuBubbleView::clear_close_on_deactivate_for_testing();
  static_cast<views::MenuButtonListener*>(button)->OnMenuButtonClicked(
      NULL, gfx::Point());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(AvatarMenuBubbleView::IsShowing());
}

// See http://crbug.com/315732
#if defined(OS_WIN)
#define MAYBE_HideOnSecondClick DISABLED_HideOnSecondClick
#elif defined(OS_CHROMEOS)
// This test doesn't make sense for ChromeOS since it has a different
// multi-profiles menu in the system tray instead.
#define MAYBE_HideOnSecondClick DISABLED_HideOnSecondClick
#else
#define MAYBE_HideOnSecondClick HideOnSecondClick
#endif

IN_PROC_BROWSER_TEST_F(AvatarMenuButtonTest, MAYBE_HideOnSecondClick) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  CreateTestingProfile();
  ASSERT_NO_FATAL_FAILURE(StartAvatarMenu());

  // Verify that clicking again doesn't reshow it.
  AvatarMenuButton* button = GetAvatarMenuButton();
  static_cast<views::MenuButtonListener*>(button)->OnMenuButtonClicked(
      NULL, gfx::Point());
  // Hide the bubble manually. In the browser this would normally happen during
  // the event processing.
  AvatarMenuBubbleView::Hide();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(AvatarMenuBubbleView::IsShowing());
}

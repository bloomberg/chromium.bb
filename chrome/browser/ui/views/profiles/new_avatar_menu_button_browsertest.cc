// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/profiles/new_avatar_button.h"
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/test/test_utils.h"
#include "grit/generated_resources.h"
#include "ui/views/controls/button/label_button.h"

class NewAvatarMenuButtonTest : public InProcessBrowserTest {
 public:
  NewAvatarMenuButtonTest();
  virtual ~NewAvatarMenuButtonTest();

 protected:
  virtual void SetUp() OVERRIDE;
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  void CreateTestingProfile();
  void StartAvatarMenu();

 private:
  DISALLOW_COPY_AND_ASSIGN(NewAvatarMenuButtonTest);
};

NewAvatarMenuButtonTest::NewAvatarMenuButtonTest() {
}

NewAvatarMenuButtonTest::~NewAvatarMenuButtonTest() {
}

void NewAvatarMenuButtonTest::SetUp() {
  InProcessBrowserTest::SetUp();
  DCHECK(switches::IsNewAvatarMenu());
}

void NewAvatarMenuButtonTest::SetUpCommandLine(CommandLine* command_line) {
  switches::EnableNewAvatarMenuForTesting(command_line);
}

void NewAvatarMenuButtonTest::CreateTestingProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(1u, profile_manager->GetNumberOfProfiles());

  // Sign in the default profile
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  cache.SetUserNameOfProfileAtIndex(0, base::UTF8ToUTF16("user_name"));

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

void NewAvatarMenuButtonTest::StartAvatarMenu() {
  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());

  // Ensure that the avatar icon button is not also showing.
  NewAvatarButton* button = browser_view->frame()->GetNewAvatarMenuButton();
  ASSERT_TRUE(button);
  ASSERT_FALSE(browser_view->frame()->GetAvatarMenuButton());

  ProfileChooserView::clear_close_on_deactivate_for_testing();
  ui::MouseEvent mouse_ev(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), 0,
                          0);
  button->NotifyClick(mouse_ev);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(ProfileChooserView::IsShowing());
}

#if defined(OS_CHROMEOS) || defined (OS_LINUX) || defined (OS_WIN)
// This test doesn't make sense for ChromeOS since it has a different
// multi-profiles menu in the system tray instead.
//
// Test fails flakily on Linux and Windows http://crbug.com/352710
#define MAYBE_SignOut DISABLED_SignOut
#else
#define MAYBE_SignOut SignOut
#endif

IN_PROC_BROWSER_TEST_F(NewAvatarMenuButtonTest, MAYBE_SignOut) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  CreateTestingProfile();
  ASSERT_NO_FATAL_FAILURE(StartAvatarMenu());

  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::GetActiveDesktop());
  EXPECT_EQ(1U, browser_list->size());
  content::WindowedNotificationObserver window_close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser()));

  AvatarMenu* menu =
      ProfileChooserView::profile_bubble_->avatar_menu_.get();
  const AvatarMenu::Item& menu_item_before =
      menu->GetItemAt(menu->GetActiveProfileIndex());
  EXPECT_FALSE(menu_item_before.signin_required);

  ui::MouseEvent mouse_ev(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), 0, 0);
  ProfileChooserView::profile_bubble_->ButtonPressed(
      ProfileChooserView::profile_bubble_->lock_button_, mouse_ev);

  EXPECT_TRUE(menu->GetItemAt(menu->GetActiveProfileIndex()).signin_required);

  window_close_observer.Wait();  // Rely on test timeout for failure indication.
  EXPECT_TRUE(browser_list->empty());

  // If the User Manager hasn't shown yet, wait for it to show up.
  if (!UserManagerView::IsShowing())
    base::MessageLoop::current()->RunUntilIdle();

  // We need to hide the User Manager or else the process can't die.
  chrome::HideUserManager();
}

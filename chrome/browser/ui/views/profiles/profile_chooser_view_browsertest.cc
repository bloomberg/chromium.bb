// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"

namespace {

Profile* CreateTestingProfile(const std::string& profile_name) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t starting_number_of_profiles = profile_manager->GetNumberOfProfiles();

  base::FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.AppendASCII(profile_name);
  if (!base::PathExists(path) && !base::CreateDirectory(path))
    NOTREACHED() << "Could not create directory at " << path.MaybeAsASCII();

  Profile* profile =
      Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
  profile_manager->RegisterTestingProfile(profile, true, false);
  EXPECT_EQ(starting_number_of_profiles + 1,
            profile_manager->GetNumberOfProfiles());
  return profile;
}

Profile* CreateProfileOutsideUserDataDir() {
  base::FilePath path;
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(), &path))
    NOTREACHED() << "Could not create directory at " << path.MaybeAsASCII();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile =
      Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
  profile_manager->RegisterTestingProfile(profile, true, false);
  return profile;
}

// Set up the profiles to enable Lock. Takes as parameter a profile that will be
// signed in, and also creates a supervised user (necessary for lock).
void SetupProfilesForLock(Profile* signed_in) {
  const char signed_in_email[] = "me@google.com";

  // Set up the |signed_in| profile.
  ProfileAttributesStorage* storage =
      &g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry_signed_in;
  ASSERT_TRUE(storage->GetProfileAttributesWithPath(signed_in->GetPath(),
                                                    &entry_signed_in));
  entry_signed_in->SetAuthInfo("12345", base::UTF8ToUTF16(signed_in_email));
  signed_in->GetPrefs()->SetString(prefs::kGoogleServicesHostedDomain,
                                   "google.com");

  // Create the |supervised| profile, which is supervised by |signed_in|.
  ProfileAttributesEntry* entry_supervised;
  Profile* supervised = CreateTestingProfile("supervised");
  ASSERT_TRUE(storage->GetProfileAttributesWithPath(supervised->GetPath(),
                                                    &entry_supervised));
  entry_supervised->SetSupervisedUserId(signed_in_email);

  // |signed_in| should now be lockable.
  EXPECT_TRUE(profiles::IsLockAvailable(signed_in));
}

views::View* FindWebView(views::View* view) {
  std::queue<views::View*> queue;
  queue.push(view);
  while (!queue.empty()) {
    views::View* current = queue.front();
    queue.pop();
    if (current->GetClassName() == views::WebView::kViewClassName)
      return current;

    for (int i = 0; i < current->child_count(); ++i)
      queue.push(current->child_at(i));
  }
  return nullptr;
}

}  // namespace

class ProfileChooserViewExtensionsTest : public ExtensionBrowserTest {
 public:
  ProfileChooserViewExtensionsTest() {}
  ~ProfileChooserViewExtensionsTest() override {}

 protected:
  void SetUp() override {
    ExtensionBrowserTest::SetUp();
    DCHECK(switches::IsNewProfileManagement());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    switches::EnableNewProfileManagementForTesting(command_line);
  }

  void OpenProfileChooserView(Browser* browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    views::View* button = browser_view->frame()->GetNewAvatarMenuButton();
    if (!button)
      NOTREACHED() << "NewAvatarButton not found.";

    ProfileChooserView::close_on_deactivate_for_testing_ = false;

    ui::MouseEvent e(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    button->OnMouseReleased(e);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(ProfileChooserView::IsShowing());

    // Create this observer before lock is pressed to avoid a race condition.
    window_close_observer_.reset(new content::WindowedNotificationObserver(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(browser)));
  }

  AvatarMenu* GetProfileChooserViewAvatarMenu() {
    return ProfileChooserView::profile_bubble_->avatar_menu_.get();
  }

  void ClickProfileChooserViewLockButton() {
    ui::MouseEvent e(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    ProfileChooserView::profile_bubble_->ButtonPressed(
        ProfileChooserView::profile_bubble_->lock_button_, e);
  }

  // Access the registry that has been prepared with at least one extension.
  extensions::ExtensionRegistry* GetPreparedRegistry(Profile* signed_in) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(signed_in);
    const size_t initial_num_extensions = registry->enabled_extensions().size();
    const extensions::Extension* ext = LoadExtension(
        test_data_dir_.AppendASCII("app"));
    EXPECT_TRUE(ext);
    EXPECT_EQ(initial_num_extensions + 1,
              registry->enabled_extensions().size());
    EXPECT_EQ(0U, registry->blocked_extensions().size());
    return registry;
  }

  content::WindowedNotificationObserver* window_close_observer() {
    return window_close_observer_.get();
  }

  ProfileChooserView* current_profile_bubble() {
    return ProfileChooserView::profile_bubble_;
  }

  views::View* signin_current_profile_button() {
    return ProfileChooserView::profile_bubble_->signin_current_profile_button_;
  }

  void ShowSigninView() {
    DCHECK(!switches::UsePasswordSeparatedSigninFlow());
    DCHECK(current_profile_bubble());
    DCHECK(current_profile_bubble()->avatar_menu_);
    current_profile_bubble()->ShowView(
        profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN,
        current_profile_bubble()->avatar_menu_.get());
    base::RunLoop().RunUntilIdle();
  }

 private:
  std::unique_ptr<content::WindowedNotificationObserver> window_close_observer_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserViewExtensionsTest);
};

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
    NoProfileChooserOnOutsideUserDataDirProfiles) {
  // Test that the profile chooser view does not show when avatar menu is not
  // available. This can be repro'ed with a profile path outside user_data_dir.
  // crbug.com/527505
  Profile* new_profile = CreateProfileOutsideUserDataDir();
  Browser* browser = CreateBrowser(new_profile);
  browser->window()->ShowAvatarBubbleFromAvatarButton(
      BrowserWindow::AVATAR_BUBBLE_MODE_CONFIRM_SIGNIN,
      signin::ManageAccountsParams(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN, false);
  ASSERT_FALSE(ProfileChooserView::IsShowing());
  CloseBrowserSynchronously(browser);
}

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, SigninButtonHasFocus) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));

  EXPECT_TRUE(signin_current_profile_button()->HasFocus());
}

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, ContentAreaHasFocus) {
  // The ProfileChooserView doesn't handle sign in under the new password
  // separated signin flow.
  if (switches::UsePasswordSeparatedSigninFlow())
    return;

  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));

  ShowSigninView();

  ASSERT_TRUE(current_profile_bubble());
  views::View* web_view = FindWebView(current_profile_bubble());
  ASSERT_TRUE(web_view);
  EXPECT_TRUE(web_view->HasFocus());
}

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, ViewProfileUMA) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());

  base::HistogramTester histograms;
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(prefs::kProfileAvatarTutorialShown, 0);

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));

  // The MD user menu doesn't display any upgrade toast so it doesn't log this
  // in UMA.
  if (!switches::IsMaterialDesignUserMenu()) {
    histograms.ExpectUniqueSample("Profile.NewAvatarMenu.Upgrade",
        ProfileMetrics::PROFILE_AVATAR_MENU_UPGRADE_VIEW, 1);
  }
}

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, LockProfile) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());

  // Set up the message loop for the user manager.
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  UserManager::AddOnUserManagerShownCallbackForTesting(runner->QuitClosure());

  SetupProfilesForLock(browser()->profile());
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
  AvatarMenu* menu = GetProfileChooserViewAvatarMenu();
  EXPECT_FALSE(menu->GetItemAt(menu->GetActiveProfileIndex()).signin_required);

  ClickProfileChooserViewLockButton();
  EXPECT_TRUE(menu->GetItemAt(menu->GetActiveProfileIndex()).signin_required);

  window_close_observer()->Wait();
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Wait until the user manager is shown.
  runner->Run();

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       LockProfileBlockExtensions) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  // Make sure we have at least one enabled extension.
  extensions::ExtensionRegistry* registry =
      GetPreparedRegistry(browser()->profile());

  // Set up the message loop for the user manager.
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  UserManager::AddOnUserManagerShownCallbackForTesting(runner->QuitClosure());

  SetupProfilesForLock(browser()->profile());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
  ClickProfileChooserViewLockButton();
  window_close_observer()->Wait();

  // Wait until the user manager is shown.
  runner->Run();

  // Assert that the ExtensionService is blocked.
  ASSERT_EQ(1U, registry->blocked_extensions().size());

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       LockProfileNoBlockOtherProfileExtensions) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  // Make sure we have at least one enabled extension.
  extensions::ExtensionRegistry* registry =
      GetPreparedRegistry(browser()->profile());
  const size_t total_enabled_extensions = registry->enabled_extensions().size();

  // Set up the message loop for the user manager.
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  UserManager::AddOnUserManagerShownCallbackForTesting(runner->QuitClosure());

  // Create a different profile and then lock it.
  Profile *signed_in = CreateTestingProfile("signed_in");
  SetupProfilesForLock(signed_in);
  extensions::ExtensionSystem::Get(signed_in)->InitForRegularProfile(true);
  Browser* browser_to_lock = CreateBrowser(signed_in);
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser_to_lock));
  ClickProfileChooserViewLockButton();
  window_close_observer()->Wait();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Wait until the user manager is shown.
  runner->Run();

  // Assert that the first profile's extensions are not blocked.
  ASSERT_EQ(total_enabled_extensions, registry->enabled_extensions().size());
  ASSERT_EQ(0U, registry->blocked_extensions().size());

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

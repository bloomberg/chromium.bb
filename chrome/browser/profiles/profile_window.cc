// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_window.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/pref_service_flags_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"

#if !defined(OS_IOS)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#endif  // !defined (OS_IOS)

using base::UserMetricsAction;
using content::BrowserThread;

namespace {

const char kNewProfileManagementExperimentInternalName[] =
    "enable-new-profile-management";

// Handles running a callback when a new Browser for the given profile
// has been completely created.
class BrowserAddedForProfileObserver : public chrome::BrowserListObserver {
 public:
  BrowserAddedForProfileObserver(
      Profile* profile,
      profiles::ProfileSwitchingDoneCallback callback)
      : profile_(profile),
        callback_(callback) {
    DCHECK(!callback_.is_null());
    BrowserList::AddObserver(this);
  }
  virtual ~BrowserAddedForProfileObserver() {
  }

 private:
  // Overridden from BrowserListObserver:
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE {
    if (browser->profile() == profile_) {
      BrowserList::RemoveObserver(this);
      callback_.Run();
      base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    }
  }

  // Profile for which the browser should be opened.
  Profile* profile_;
  profiles::ProfileSwitchingDoneCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAddedForProfileObserver);
};

void OpenBrowserWindowForProfile(
    profiles::ProfileSwitchingDoneCallback callback,
    bool always_create,
    bool is_new_profile,
    chrome::HostDesktopType desktop_type,
    Profile* profile,
    Profile::CreateStatus status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  chrome::startup::IsProcessStartup is_process_startup =
      chrome::startup::IS_NOT_PROCESS_STARTUP;
  chrome::startup::IsFirstRun is_first_run = chrome::startup::IS_NOT_FIRST_RUN;

  // If this is a brand new profile, then start a first run window.
  if (is_new_profile) {
    is_process_startup = chrome::startup::IS_PROCESS_STARTUP;
    is_first_run = chrome::startup::IS_FIRST_RUN;
  }

  // If |always_create| is false, and we have a |callback| to run, check
  // whether a browser already exists so that we can run the callback. We don't
  // want to rely on the observer listening to OnBrowserSetLastActive in this
  // case, as you could manually activate an incorrect browser and trigger
  // a false positive.
  if (!always_create) {
    Browser* browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
    if (browser) {
      browser->window()->Activate();
      if (!callback.is_null())
        callback.Run();
      return;
    }
  }

  // If there is a callback, create an observer to make sure it is only
  // run when the browser has been completely created. This observer will
  // delete itself once that happens. This should not leak, because we are
  // passing |always_create| = true to FindOrCreateNewWindow below, which ends
  // up calling LaunchBrowser and opens a new window. If for whatever reason
  // that fails, either something has crashed, or the observer will be cleaned
  // up when a different browser for this profile is opened.
  if (!callback.is_null())
    new BrowserAddedForProfileObserver(profile, callback);

  // We already dealt with the case when |always_create| was false and a browser
  // existed, which means that here a browser definitely needs to be created.
  // Passing true for |always_create| means we won't duplicate the code that
  // tries to find a browser.
  profiles::FindOrCreateNewWindowForProfile(
      profile,
      is_process_startup,
      is_first_run,
      desktop_type,
      true);
}

// Called after a |guest_profile| is available to be used by the user manager.
// Based on the value of |tutorial_mode| we determine a url to be displayed
// by the webui and run the |callback|, if it exists.
void OnUserManagerGuestProfileCreated(
    const base::FilePath& profile_path_to_focus,
    profiles::UserManagerTutorialMode tutorial_mode,
    const base::Callback<void(Profile*, const std::string&)>& callback,
    Profile* guest_profile,
    Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_INITIALIZED || callback.is_null())
    return;

  // Tell the webui which user should be focused.
  std::string page = chrome::kChromeUIUserManagerURL;

  if (tutorial_mode == profiles::USER_MANAGER_TUTORIAL_OVERVIEW) {
    page += "#tutorial";
  } else if (!profile_path_to_focus.empty()) {
    const ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t index = cache.GetIndexOfProfileWithPath(profile_path_to_focus);
    if (index != std::string::npos) {
      page += "#";
      page += base::IntToString(index);
    }
  }

  callback.Run(guest_profile, page);
}

// Updates Chrome services that require notification when
// the new_profile_management's status changes.
void UpdateServicesWithNewProfileManagementFlag(Profile* profile,
                                                bool new_flag_status) {
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  account_reconcilor->OnNewProfileManagementFlagChanged(new_flag_status);
}

}  // namespace

namespace profiles {

void FindOrCreateNewWindowForProfile(
    Profile* profile,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    chrome::HostDesktopType desktop_type,
    bool always_create) {
#if defined(OS_IOS)
  NOTREACHED();
#else
  DCHECK(profile);

  if (!always_create) {
    Browser* browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
    if (browser) {
      browser->window()->Activate();
      return;
    }
  }

  content::RecordAction(UserMetricsAction("NewWindow"));
  CommandLine command_line(CommandLine::NO_PROGRAM);
  int return_code;
  StartupBrowserCreator browser_creator;
  browser_creator.LaunchBrowser(command_line, profile, base::FilePath(),
                                process_startup, is_first_run, &return_code);
#endif  // defined(OS_IOS)
}

void SwitchToProfile(const base::FilePath& path,
                     chrome::HostDesktopType desktop_type,
                     bool always_create,
                     ProfileSwitchingDoneCallback callback,
                     ProfileMetrics::ProfileOpen metric) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      path,
      base::Bind(&OpenBrowserWindowForProfile,
                 callback,
                 always_create,
                 false,
                 desktop_type),
      base::string16(),
      base::string16(),
      std::string());
  ProfileMetrics::LogProfileSwitchUser(metric);
}

void SwitchToGuestProfile(chrome::HostDesktopType desktop_type,
                          ProfileSwitchingDoneCallback callback) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileManager::GetGuestProfilePath(),
      base::Bind(&OpenBrowserWindowForProfile,
                 callback,
                 false,
                 false,
                 desktop_type),
      base::string16(),
      base::string16(),
      std::string());
  ProfileMetrics::LogProfileSwitchUser(ProfileMetrics::SWITCH_PROFILE_GUEST);
}

void CreateAndSwitchToNewProfile(chrome::HostDesktopType desktop_type,
                                 ProfileSwitchingDoneCallback callback,
                                 ProfileMetrics::ProfileAdd metric) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();

  int placeholder_avatar_index = profiles::GetPlaceholderAvatarIndex();
  ProfileManager::CreateMultiProfileAsync(
      cache.ChooseNameForNewProfile(placeholder_avatar_index),
      base::UTF8ToUTF16(profiles::GetDefaultAvatarIconUrl(
          placeholder_avatar_index)),
      base::Bind(&OpenBrowserWindowForProfile,
                 callback,
                 true,
                 true,
                 desktop_type),
      std::string());
  ProfileMetrics::LogProfileAddNewUser(metric);
}

void CloseGuestProfileWindows() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());

  if (profile) {
    BrowserList::CloseAllBrowsersWithProfile(profile);
  }
}

void LockProfile(Profile* profile) {
  DCHECK(profile);
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();

  size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  cache.SetProfileSigninRequiredAtIndex(index, true);
  chrome::ShowUserManager(profile->GetPath());
  BrowserList::CloseAllBrowsersWithProfile(profile);
}

void CreateGuestProfileForUserManager(
    const base::FilePath& profile_path_to_focus,
    profiles::UserManagerTutorialMode tutorial_mode,
    const base::Callback<void(Profile*, const std::string&)>& callback) {
  // Create the guest profile, if necessary, and open the User Manager
  // from the guest profile.
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileManager::GetGuestProfilePath(),
      base::Bind(&OnUserManagerGuestProfileCreated,
                 profile_path_to_focus,
                 tutorial_mode,
                 callback),
      base::string16(),
      base::string16(),
      std::string());
}

void ShowUserManagerMaybeWithTutorial(Profile* profile) {
  // Guest users cannot appear in the User Manager, nor display a tutorial.
  if (!profile || profile->IsGuestSession()) {
    chrome::ShowUserManager(base::FilePath());
    return;
  }
  // Show the tutorial if the profile has not shown it before.
  PrefService* pref_service = profile->GetPrefs();
  bool tutorial_shown = pref_service->GetBoolean(
      prefs::kProfileUserManagerTutorialShown);
  if (!tutorial_shown)
    pref_service->SetBoolean(prefs::kProfileUserManagerTutorialShown, true);

  if (tutorial_shown) {
    chrome::ShowUserManager(profile->GetPath());
  } else {
    chrome::ShowUserManagerWithTutorial(
        profiles::USER_MANAGER_TUTORIAL_OVERVIEW);
  }
}

void EnableNewProfileManagementPreview(Profile* profile) {
#if defined(OS_ANDROID)
  NOTREACHED();
#else
  // TODO(rogerta): instead of setting experiment flags and command line
  // args, we should set a profile preference.
  const about_flags::Experiment experiment = {
      kNewProfileManagementExperimentInternalName,
      0,  // string id for title of experiment
      0,  // string id for description of experiment
      0,  // supported platforms
      about_flags::Experiment::ENABLE_DISABLE_VALUE,
      switches::kEnableNewProfileManagement,
      "",  // not used with ENABLE_DISABLE_VALUE type
      switches::kDisableNewProfileManagement,
      "",  // not used with ENABLE_DISABLE_VALUE type
      NULL,  // not used with ENABLE_DISABLE_VALUE type
      3
  };
  about_flags::PrefServiceFlagsStorage flags_storage(
      g_browser_process->local_state());
  about_flags::SetExperimentEnabled(
      &flags_storage,
      experiment.NameForChoice(1),
      true);

  switches::EnableNewProfileManagementForTesting(
      CommandLine::ForCurrentProcess());
  chrome::ShowUserManagerWithTutorial(profiles::USER_MANAGER_TUTORIAL_OVERVIEW);
  UpdateServicesWithNewProfileManagementFlag(profile, true);
#endif
}

void DisableNewProfileManagementPreview(Profile* profile) {
  about_flags::PrefServiceFlagsStorage flags_storage(
      g_browser_process->local_state());
  about_flags::SetExperimentEnabled(
      &flags_storage,
      kNewProfileManagementExperimentInternalName,
      false);
  chrome::AttemptRestart();
  UpdateServicesWithNewProfileManagementFlag(profile, false);
}

BubbleViewMode BubbleViewModeFromAvatarBubbleMode(
    BrowserWindow::AvatarBubbleMode mode) {
  switch (mode) {
    case BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT:
      return profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT;
    case BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN:
      return profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN;
    case BrowserWindow::AVATAR_BUBBLE_MODE_ADD_ACCOUNT:
      return profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT;
    case BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH:
      return profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH;
    case BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT:
      return profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
  }
  NOTREACHED();
  return profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
}

}  // namespace profiles

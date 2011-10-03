// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/profiles/profile_manager.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#endif

namespace {

void DeleteProfileDirectories(const std::vector<FilePath>& paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  for (std::vector<FilePath>::const_iterator it = paths.begin();
       it != paths.end(); ++it) {
    file_util::Delete(*it, true);
  }
}

} // namespace

bool ProfileManagerObserver::DeleteAfter() {
  return false;
}

// The NewProfileLauncher class is created when to wait for a multi-profile
// to be created asynchronously. Upon completion of profile creation, the
// NPL takes care of launching a new browser window and signing the user
// in to their Google account.
class NewProfileLauncher : public ProfileManagerObserver {
 public:
  virtual void OnProfileCreated(Profile* profile, Status status) {
    if (status == STATUS_INITIALIZED) {
      DCHECK(profile);
      Browser* browser = Browser::Create(profile);
      browser->AddSelectedTabWithURL(GURL(chrome::kChromeUINewTabURL),
                                     PageTransition::LINK);
      browser->window()->Show();
    }
  }

  virtual bool DeleteAfter() OVERRIDE { return true; }
};

// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  if (!pm)  // Is NULL when running unit tests.
    return;
  std::vector<Profile*> profiles(pm->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i)
    SessionServiceFactory::ShutdownForProfile(profiles[i]);
}

// static
Profile* ProfileManager::GetDefaultProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetDefaultProfile(profile_manager->user_data_dir_);
}

// static
Profile* ProfileManager::GetLastUsedProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastUsedProfile(profile_manager->user_data_dir_);
}

ProfileManager::ProfileManager(const FilePath& user_data_dir)
    : user_data_dir_(user_data_dir),
      logged_in_(false),
      will_import_(false) {
  BrowserList::AddObserver(this);
#if defined(OS_CHROMEOS)
  registrar_.Add(
      this,
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      NotificationService::AllSources());
#endif
}

ProfileManager::~ProfileManager() {
  BrowserList::RemoveObserver(this);

  // TODO(sail): fix http://crbug.com/88586
  if (profiles_to_delete_.size()) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(&DeleteProfileDirectories, profiles_to_delete_));
    profiles_to_delete_.clear();
  }
}

FilePath ProfileManager::GetDefaultProfileDir(
    const FilePath& user_data_dir) {
  FilePath default_profile_dir(user_data_dir);
  default_profile_dir =
      default_profile_dir.AppendASCII(chrome::kInitialProfile);
  return default_profile_dir;
}

FilePath ProfileManager::GetProfilePrefsPath(
    const FilePath &profile_dir) {
  FilePath default_prefs_path(profile_dir);
  default_prefs_path = default_prefs_path.Append(chrome::kPreferencesFilename);
  return default_prefs_path;
}

FilePath ProfileManager::GetInitialProfileDir() {
  FilePath relative_profile_dir;
#if defined(OS_CHROMEOS)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (logged_in_) {
    FilePath profile_dir;
    // If the user has logged in, pick up the new profile.
    if (command_line.HasSwitch(switches::kLoginProfile)) {
      profile_dir = command_line.GetSwitchValuePath(switches::kLoginProfile);
    } else {
      // We should never be logged in with no profile dir.
      NOTREACHED();
      return FilePath("");
    }
    relative_profile_dir = relative_profile_dir.Append(profile_dir);
    return relative_profile_dir;
  }
#endif
  // TODO(mirandac): should not automatically be default profile.
  relative_profile_dir =
      relative_profile_dir.AppendASCII(chrome::kInitialProfile);
  return relative_profile_dir;
}

Profile* ProfileManager::GetLastUsedProfile(const FilePath& user_data_dir) {
  FilePath last_used_profile_dir(user_data_dir);
  std::string last_profile_used;
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  if (local_state->HasPrefPath(prefs::kProfileLastUsed))
    last_profile_used = local_state->GetString(prefs::kProfileLastUsed);
  last_used_profile_dir = last_profile_used.empty() ?
      last_used_profile_dir.AppendASCII(chrome::kInitialProfile) :
      last_used_profile_dir.AppendASCII(last_profile_used);
  return GetProfile(last_used_profile_dir);
}

Profile* ProfileManager::GetDefaultProfile(const FilePath& user_data_dir) {
  FilePath default_profile_dir(user_data_dir);
  default_profile_dir = default_profile_dir.Append(GetInitialProfileDir());
#if defined(OS_CHROMEOS)
  if (!logged_in_) {
    Profile* profile = GetProfile(default_profile_dir);
    // For cros, return the OTR profile so we never accidentally keep
    // user data in an unencrypted profile. But doing this makes
    // many of the browser and ui tests fail. We do return the OTR profile
    // if the login-profile switch is passed so that we can test this.
    // TODO(davemoore) Fix the tests so they allow OTR profiles.
    if (ShouldGoOffTheRecord())
      return profile->GetOffTheRecordProfile();
    return profile;
  }
#endif
  return GetProfile(default_profile_dir);
}

bool ProfileManager::IsValidProfile(Profile* profile) {
  for (ProfilesInfoMap::iterator iter = profiles_info_.begin();
       iter != profiles_info_.end(); ++iter) {
    if (iter->second->created) {
      Profile* candidate = iter->second->profile.get();
      if (candidate == profile ||
          (candidate->HasOffTheRecordProfile() &&
           candidate->GetOffTheRecordProfile() == profile)) {
        return true;
      }
    }
  }
  return false;
}

std::vector<Profile*> ProfileManager::GetLoadedProfiles() const {
  std::vector<Profile*> profiles;
  for (ProfilesInfoMap::const_iterator iter = profiles_info_.begin();
       iter != profiles_info_.end(); ++iter) {
    if (iter->second->created)
      profiles.push_back(iter->second->profile.get());
  }
  return profiles;
}

Profile* ProfileManager::GetProfile(const FilePath& profile_dir) {
  // If the profile is already loaded (e.g., chrome.exe launched twice), just
  // return it.
  Profile* profile = GetProfileByPath(profile_dir);
  if (NULL != profile)
    return profile;

  profile = CreateProfile(profile_dir);
  DCHECK(profile);
  if (profile) {
    bool result = AddProfile(profile);
    DCHECK(result);
  }
  return profile;
}

void ProfileManager::CreateProfileAsync(const FilePath& user_data_dir,
                                        ProfileManagerObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProfilesInfoMap::iterator iter = profiles_info_.find(user_data_dir);
  if (iter != profiles_info_.end()) {
    ProfileInfo* info = iter->second.get();
    if (info->created) {
      // Profile has already been created. Call observer immediately.
      observer->OnProfileCreated(
          info->profile.get(), ProfileManagerObserver::STATUS_INITIALIZED);
      if (observer->DeleteAfter())
        delete observer;
    } else {
      // Profile is being created. Add observer to list.
      info->observers.push_back(observer);
    }
  } else {
    // Initiate asynchronous creation process.
    ProfileInfo* info =
        RegisterProfile(Profile::CreateProfileAsync(user_data_dir, this),
                        false);
    info->observers.push_back(observer);
  }
}

// static
void ProfileManager::CreateDefaultProfileAsync(
    ProfileManagerObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  FilePath default_profile_dir = profile_manager->user_data_dir_;
  // TODO(mirandac): current directory will not always be default in the future
  default_profile_dir = default_profile_dir.Append(
      profile_manager->GetInitialProfileDir());

  profile_manager->CreateProfileAsync(default_profile_dir,
                                      observer);
}

bool ProfileManager::AddProfile(Profile* profile) {
  DCHECK(profile);

  // Make sure that we're not loading a profile with the same ID as a profile
  // that's already loaded.
  if (GetProfileByPath(profile->GetPath())) {
    NOTREACHED() << "Attempted to add profile with the same path (" <<
                    profile->GetPath().value() <<
                    ") as an already-loaded profile.";
    return false;
  }

  RegisterProfile(profile, true);
  DoFinalInit(profile, ShouldGoOffTheRecord());
  return true;
}

ProfileManager::ProfileInfo* ProfileManager::RegisterProfile(Profile* profile,
                                                             bool created) {
  ProfileInfo* info = new ProfileInfo(profile, created);
  profiles_info_.insert(std::make_pair(profile->GetPath(), info));
  return info;
}

Profile* ProfileManager::GetProfileByPath(const FilePath& path) const {
  ProfilesInfoMap::const_iterator iter = profiles_info_.find(path);
  return (iter == profiles_info_.end()) ? NULL : iter->second->profile.get();
}

void ProfileManager::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (chromeos::CrosLibrary::Get()->EnsureLoaded() &&
        !command_line.HasSwitch(switches::kTestType)) {
      // If we don't have a mounted profile directory we're in trouble.
      // TODO(davemoore) Once we have better api this check should ensure that
      // our profile directory is the one that's mounted, and that it's mounted
      // as the current user.
      CHECK(chromeos::CrosLibrary::Get()->GetCryptohomeLibrary()->IsMounted());
    }
    logged_in_ = true;
  }
#endif
}

void ProfileManager::SetWillImport() {
  will_import_ = true;
}

void ProfileManager::OnImportFinished(Profile* profile) {
  will_import_ = false;
  DCHECK(profile);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_IMPORT_FINISHED,
      Source<Profile>(profile),
      NotificationService::NoDetails());
}

void ProfileManager::OnBrowserAdded(const Browser* browser) {}

void ProfileManager::OnBrowserRemoved(const Browser* browser) {}

void ProfileManager::OnBrowserSetLastActive(const Browser* browser) {
  Profile* last_active = browser->GetProfile();
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Only keep track of profiles that we are managing; tests may create others.
  if (profiles_info_.find(last_active->GetPath()) != profiles_info_.end()) {
    local_state->SetString(prefs::kProfileLastUsed,
                           last_active->GetPath().BaseName().MaybeAsASCII());
  }
}

void ProfileManager::DoFinalInit(Profile* profile, bool go_off_the_record) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  profile->InitExtensions(!go_off_the_record);
  if (!command_line.HasSwitch(switches::kDisableWebResources))
    profile->InitPromoResources();
  AddProfileToCache(profile);
}

Profile* ProfileManager::CreateProfile(const FilePath& path) {
  return Profile::CreateProfile(path);
}

void ProfileManager::OnProfileCreated(Profile* profile, bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ProfilesInfoMap::iterator iter = profiles_info_.find(profile->GetPath());
  DCHECK(iter != profiles_info_.end());
  ProfileInfo* info = iter->second.get();

  std::vector<ProfileManagerObserver*> observers;
  info->observers.swap(observers);

  bool go_off_the_record = ShouldGoOffTheRecord();
  if (success) {
    if (!go_off_the_record) {
      for (size_t i = 0; i < observers.size(); ++i) {
        observers[i]->OnProfileCreated(
            profile, ProfileManagerObserver::STATUS_CREATED);
      }
    }
    DoFinalInit(profile, go_off_the_record);
    info->created = true;
  } else {
    profile = NULL;
    profiles_info_.erase(iter);
  }

  std::vector<ProfileManagerObserver*> observers_to_delete;

  for (size_t i = 0; i < observers.size(); ++i) {
    if (profile && go_off_the_record) {
      profile = profile->GetOffTheRecordProfile();
      DCHECK(profile);
      observers[i]->OnProfileCreated(
          profile, ProfileManagerObserver::STATUS_CREATED);
    }
    observers[i]->OnProfileCreated(
        profile, profile ? ProfileManagerObserver::STATUS_INITIALIZED :
            ProfileManagerObserver::STATUS_FAIL);
    if (observers[i]->DeleteAfter())
      observers_to_delete.push_back(observers[i]);
  }

  STLDeleteElements(&observers_to_delete);
}

// static
void ProfileManager::CreateMultiProfileAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Create the next profile in the next available directory slot.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  int next_directory = local_state->GetInteger(prefs::kProfilesNumCreated);
  std::string profile_name = chrome::kMultiProfileDirPrefix;
  profile_name.append(base::IntToString(next_directory));
  FilePath new_path = profile_manager->user_data_dir_;
#if defined(OS_WIN)
  new_path = new_path.Append(ASCIIToUTF16(profile_name));
#else
  new_path = new_path.Append(profile_name);
#endif
  local_state->SetInteger(prefs::kProfilesNumCreated, ++next_directory);

  // The launcher is deleted by the manager when profile creation is finished.
  NewProfileLauncher* launcher = new NewProfileLauncher();
  profile_manager->CreateProfileAsync(new_path, launcher);
}

// static
void ProfileManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kProfileLastUsed, "");
  prefs->RegisterIntegerPref(prefs::kProfilesNumCreated, 1);
}

size_t ProfileManager::GetNumberOfProfiles() {
  return GetProfileInfoCache().GetNumberOfProfiles();
}

bool ProfileManager::CompareProfilePathAndName(
    const ProfileManager::ProfilePathAndName& pair1,
    const ProfileManager::ProfilePathAndName& pair2) {
  int name_compare = pair1.second.compare(pair2.second);
  if (name_compare < 0) {
    return true;
  } else if (name_compare > 0) {
    return false;
  } else {
    return pair1.first < pair2.first;
  }
}

ProfileInfoCache& ProfileManager::GetProfileInfoCache() {
  if (!profile_info_cache_.get()) {
    profile_info_cache_.reset(new ProfileInfoCache(
        g_browser_process->local_state(), user_data_dir_));
  }
  return *profile_info_cache_.get();
}

void ProfileManager::AddProfileToCache(Profile* profile) {
  ProfileInfoCache& cache = GetProfileInfoCache();
  if (profile->GetPath().DirName() != cache.GetUserDataDir())
    return;

  if (cache.GetIndexOfProfileWithPath(profile->GetPath()) != std::string::npos)
    return;

  if (profile->GetPath() == GetDefaultProfileDir(cache.GetUserDataDir())) {
    cache.AddProfileToCache(
        profile->GetPath(),
        l10n_util::GetStringUTF16(IDS_DEFAULT_PROFILE_NAME), 0);
  } else {
    cache.AddProfileToCache(
        profile->GetPath(),
        cache.ChooseNameForNewProfile(),
        cache.ChooseAvatarIconIndexForNewProfile());
  }
}

bool ProfileManager::ShouldGoOffTheRecord() {
  bool go_off_the_record = false;
#if defined(OS_CHROMEOS)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!logged_in_ &&
      (!command_line.HasSwitch(switches::kTestType) ||
       command_line.HasSwitch(switches::kLoginProfile))) {
    go_off_the_record = true;
  }
#endif
  return go_off_the_record;
}

void ProfileManager::ScheduleProfileForDeletion(const FilePath& profile_dir) {
  // TODO(sail): Due to bug 88586 we don't delete the profile instance. Once we
  // start deleting the profile instance we need to close background apps too.
  Profile* profile = GetProfileByPath(profile_dir);
  if (profile)
    BrowserList::CloseAllBrowsersWithProfile(profile);
  profiles_to_delete_.push_back(profile_dir);
  ProfileInfoCache& cache = GetProfileInfoCache();
  cache.DeleteProfileFromCache(profile_dir);
}

// static
bool ProfileManager::IsMultipleProfilesEnabled() {
#if defined(OS_MACOSX) || (defined(TOOLKIT_VIEWS) && !defined(OS_CHROMEOS))
  return true;
#else
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kMultiProfiles);
#endif
}

ProfileManagerWithoutInit::ProfileManagerWithoutInit(
    const FilePath& user_data_dir) : ProfileManager(user_data_dir) {
}

void ProfileManager::RegisterTestingProfile(Profile* profile,
                                            bool add_to_cache) {
  RegisterProfile(profile, true);
  if (add_to_cache)
    AddProfileToCache(profile);
}

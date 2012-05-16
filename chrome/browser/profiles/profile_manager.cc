// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/profiles/profile_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/managed_mode.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

using content::BrowserThread;
using content::UserMetricsAction;

namespace {

// Profiles that should be deleted on shutdown.
std::vector<FilePath>& ProfilesToDelete() {
  CR_DEFINE_STATIC_LOCAL(std::vector<FilePath>, profiles_to_delete, ());
  return profiles_to_delete;
}

// Checks if any user prefs for |profile| have default values.
bool HasAnyDefaultUserPrefs(Profile* profile) {
  const PrefService::Preference* avatar_index =
      profile->GetPrefs()->FindPreference(prefs::kProfileAvatarIndex);
  DCHECK(avatar_index);
  const PrefService::Preference* profile_name =
    profile->GetPrefs()->FindPreference(prefs::kProfileName);
  DCHECK(profile_name);
  return avatar_index->IsDefaultValue() ||
         profile_name->IsDefaultValue();
}

// Simple task to log the size of the current profile.
void ProfileSizeTask(const FilePath& path, int extension_count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  int64 size = file_util::ComputeFilesSize(path, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalSize", size_MB);

  size = file_util::ComputeFilesSize(path, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.HistorySize", size_MB);

  size = file_util::ComputeFilesSize(path, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalHistorySize", size_MB);

  size = file_util::ComputeFilesSize(path, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.CookiesSize", size_MB);

  size = file_util::ComputeFilesSize(path, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.BookmarksSize", size_MB);

  size = file_util::ComputeFilesSize(path, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.FaviconsSize", size_MB);

  size = file_util::ComputeFilesSize(path, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.TopSitesSize", size_MB);

  size = file_util::ComputeFilesSize(path, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.VisitedLinksSize", size_MB);

  size = file_util::ComputeFilesSize(path, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.WebDataSize", size_MB);

  size = file_util::ComputeFilesSize(path, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.ExtensionSize", size_MB);

  // Count number of extensions in this profile, if we know.
  if (extension_count != -1) {
    UMA_HISTOGRAM_COUNTS_10000("Profile.AppCount", extension_count);

    static bool default_apps_trial_exists = base::FieldTrialList::TrialExists(
        kDefaultAppsTrialName);
    if (default_apps_trial_exists) {
      UMA_HISTOGRAM_COUNTS_10000(
          base::FieldTrial::MakeName("Profile.AppCount",
                                     kDefaultAppsTrialName),
          extension_count);
    }
  }
}

void QueueProfileDirectoryForDeletion(const FilePath& path) {
  ProfilesToDelete().push_back(path);
}

// Called upon completion of profile creation. This function takes care of
// launching a new browser window and signing the user in to their Google
// account.
void OnOpenWindowForNewProfile(Profile* profile,
                               Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    ProfileManager::FindOrCreateNewWindowForProfile(
        profile,
        browser::startup::IS_PROCESS_STARTUP,
        browser::startup::IS_FIRST_RUN,
        false);
  }
}

} // namespace

#if defined(ENABLE_SESSION_SERVICE)
// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  if (!pm)  // Is NULL when running unit tests.
    return;
  std::vector<Profile*> profiles(pm->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i)
    SessionServiceFactory::ShutdownForProfile(profiles[i]);
}
#endif

// static
void ProfileManager::NukeDeletedProfilesFromDisk() {
  for (std::vector<FilePath>::iterator it =
          ProfilesToDelete().begin();
       it != ProfilesToDelete().end();
       ++it) {
    // Delete both the profile directory and its corresponding cache.
    FilePath cache_path;
    chrome::GetUserCacheDirectory(*it, &cache_path);
    file_util::Delete(*it, true);
    file_util::Delete(cache_path, true);
  }
  ProfilesToDelete().clear();
}

// static
Profile* ProfileManager::GetDefaultProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetDefaultProfile(profile_manager->user_data_dir_);
}

// static
Profile* ProfileManager::GetDefaultProfileOrOffTheRecord() {
  // TODO (mukai,nkostylev): In the long term we should fix those cases that
  // crash on Guest mode and have only one GetDefaultProfile() method.
  Profile* profile = GetDefaultProfile();
#if defined(OS_CHROMEOS)
  if (chromeos::UserManager::Get()->IsLoggedInAsGuest())
    profile = profile->GetOffTheRecordProfile();
#endif
  return profile;
}

// static
Profile* ProfileManager::GetLastUsedProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastUsedProfile(profile_manager->user_data_dir_);
}

// static
std::vector<Profile*> ProfileManager::GetLastOpenedProfiles() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastOpenedProfiles(
      profile_manager->user_data_dir_);
}

ProfileManager::ProfileManager(const FilePath& user_data_dir)
    : user_data_dir_(user_data_dir),
      logged_in_(false),
      will_import_(false),
#if !defined(OS_ANDROID)
      ALLOW_THIS_IN_INITIALIZER_LIST(
          browser_list_observer_(this)),
#endif
      shutdown_started_(false) {
#if defined(OS_CHROMEOS)
  registrar_.Add(
      this,
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::NotificationService::AllSources());
#endif
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      content::NOTIFICATION_APP_EXITING,
      content::NotificationService::AllSources());
}

ProfileManager::~ProfileManager() {
#if defined(OS_WIN)
  if (profile_shortcut_manager_.get())
    profile_info_cache_->RemoveObserver(profile_shortcut_manager_.get());
#endif
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
#if defined(OS_CHROMEOS)
  // Use default login profile if user has not logged in yet.
  if (!logged_in_)
    return GetDefaultProfile(user_data_dir);
#endif

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

std::vector<Profile*> ProfileManager::GetLastOpenedProfiles(
    const FilePath& user_data_dir) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  std::vector<Profile*> to_return;
  if (local_state->HasPrefPath(prefs::kProfilesLastActive)) {
    const ListValue* profile_list =
        local_state->GetList(prefs::kProfilesLastActive);
    if (profile_list) {
      ListValue::const_iterator it;
      std::string profile;
      for (it = profile_list->begin(); it != profile_list->end(); ++it) {
        if (!(*it)->GetAsString(&profile) || profile.empty()) {
          LOG(WARNING) << "Invalid entry in " << prefs::kProfilesLastActive;
          continue;
        }
        to_return.push_back(GetProfile(user_data_dir.AppendASCII(profile)));
      }
    }
  }
  return to_return;
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

  profile = CreateProfileHelper(profile_dir);
  DCHECK(profile);
  if (profile) {
    bool result = AddProfile(profile);
    DCHECK(result);
  }
  return profile;
}

void ProfileManager::CreateProfileAsync(const FilePath& profile_path,
                                        const CreateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Make sure that this profile is not pending deletion.
  if (std::find(ProfilesToDelete().begin(), ProfilesToDelete().end(),
      profile_path) != ProfilesToDelete().end()) {
    callback.Run(NULL, Profile::CREATE_STATUS_FAIL);
    return;
  }

  ProfilesInfoMap::iterator iter = profiles_info_.find(profile_path);
  if (iter != profiles_info_.end()) {
    ProfileInfo* info = iter->second.get();
    if (info->created) {
      // Profile has already been created. Run callback immediately.
      callback.Run(info->profile.get(), Profile::CREATE_STATUS_INITIALIZED);
    } else {
      // Profile is being created. Add callback to list.
      info->callbacks.push_back(callback);
    }
  } else {
    // Initiate asynchronous creation process.
    ProfileInfo* info =
        RegisterProfile(CreateProfileAsyncHelper(profile_path, this), false);
    info->callbacks.push_back(callback);
  }
}

// static
void ProfileManager::CreateDefaultProfileAsync(const CreateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  FilePath default_profile_dir = profile_manager->user_data_dir_;
  // TODO(mirandac): current directory will not always be default in the future
  default_profile_dir = default_profile_dir.Append(
      profile_manager->GetInitialProfileDir());

  profile_manager->CreateProfileAsync(default_profile_dir, callback);
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

// static
void ProfileManager::FindOrCreateNewWindowForProfile(
    Profile* profile,
    browser::startup::IsProcessStartup process_startup,
    browser::startup::IsFirstRun is_first_run,
    bool always_create) {
  DCHECK(profile);

  if (!always_create) {
    Browser* browser = browser::FindTabbedBrowser(profile, false);
    if (browser) {
      browser->window()->Activate();
      return;
    }
  }

  content::RecordAction(UserMetricsAction("NewWindow"));
  CommandLine command_line(CommandLine::NO_PROGRAM);
  int return_code;
  StartupBrowserCreator browser_creator;
  browser_creator.LaunchBrowser(command_line, profile, FilePath(),
                                process_startup, is_first_run, &return_code);
}

void ProfileManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
    logged_in_ = true;

    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType)) {
      // If we don't have a mounted profile directory we're in trouble.
      // TODO(davemoore) Once we have better api this check should ensure that
      // our profile directory is the one that's mounted, and that it's mounted
      // as the current user.
      CHECK(chromeos::CrosLibrary::Get()->GetCryptohomeLibrary()->IsMounted())
          << "The cryptohome was not mounted at login.";

      // Confirm that we hadn't loaded the new profile previously.
      FilePath default_profile_dir =
          user_data_dir_.Append(GetInitialProfileDir());
      CHECK(!GetProfileByPath(default_profile_dir))
          << "The default profile was loaded before we mounted the cryptohome.";
    }
    return;
  }
#endif
  if (shutdown_started_)
    return;

  bool update_active_profiles = false;
  switch (type) {
    case content::NOTIFICATION_APP_EXITING: {
      // Ignore any browsers closing from now on.
      shutdown_started_ = true;
      break;
    }
    case chrome::NOTIFICATION_BROWSER_OPENED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      DCHECK(browser);
      Profile* profile = browser->profile();
      DCHECK(profile);
      if (!profile->IsOffTheRecord() && ++browser_counts_[profile] == 1) {
        active_profiles_.push_back(profile);
        update_active_profiles = true;
      }
      break;
    }
    case chrome::NOTIFICATION_BROWSER_CLOSED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      DCHECK(browser);
      Profile* profile = browser->profile();
      DCHECK(profile);
      if (!profile->IsOffTheRecord() && --browser_counts_[profile] == 0) {
        active_profiles_.erase(std::find(active_profiles_.begin(),
                                         active_profiles_.end(), profile));
        update_active_profiles = true;
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
  if (update_active_profiles) {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    ListPrefUpdate update(local_state, prefs::kProfilesLastActive);
    ListValue* profile_list = update.Get();

    profile_list->Clear();

    // crbug.com/120112 -> several non-incognito profiles might have the same
    // GetPath().BaseName(). In that case, we cannot restore both
    // profiles. Include each base name only once in the last active profile
    // list.
    std::set<std::string> profile_paths;
    std::vector<Profile*>::const_iterator it;
    for (it = active_profiles_.begin(); it != active_profiles_.end(); ++it) {
      std::string profile_path = (*it)->GetPath().BaseName().MaybeAsASCII();
      if (profile_paths.find(profile_path) == profile_paths.end()) {
        profile_paths.insert(profile_path);
        profile_list->Append(new StringValue(profile_path));
      }
    }
  }
}

void ProfileManager::SetWillImport() {
  will_import_ = true;
}

void ProfileManager::OnImportFinished(Profile* profile) {
  will_import_ = false;
  DCHECK(profile);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_IMPORT_FINISHED,
      content::Source<Profile>(profile),
      content::NotificationService::NoDetails());
}

#if !defined(OS_ANDROID)
ProfileManager::BrowserListObserver::BrowserListObserver(
    ProfileManager* manager)
    : profile_manager_(manager) {
  BrowserList::AddObserver(this);
}

ProfileManager::BrowserListObserver::~BrowserListObserver() {
  BrowserList::RemoveObserver(this);
}

void ProfileManager::BrowserListObserver::OnBrowserAdded(
    const Browser* browser) {}

void ProfileManager::BrowserListObserver::OnBrowserRemoved(
    const Browser* browser) {}

void ProfileManager::BrowserListObserver::OnBrowserSetLastActive(
    const Browser* browser) {
  Profile* last_active = browser->profile();
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Only keep track of profiles that we are managing; tests may create others.
  if (profile_manager_->profiles_info_.find(
      last_active->GetPath()) != profile_manager_->profiles_info_.end()) {
    local_state->SetString(prefs::kProfileLastUsed,
                           last_active->GetPath().BaseName().MaybeAsASCII());
  }
}
#endif  // !defined(OS_ANDROID)

void ProfileManager::DoFinalInit(Profile* profile, bool go_off_the_record) {
  DoFinalInitForServices(profile, go_off_the_record);
  InitProfileUserPrefs(profile);
  AddProfileToCache(profile);
  DoFinalInitLogging(profile);
#if defined(OS_WIN)
  CreateDesktopShortcut(profile);
#endif

  ProfileMetrics::LogNumberOfProfiles(this, ProfileMetrics::ADD_PROFILE_EVENT);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile),
      content::NotificationService::NoDetails());

}

void ProfileManager::DoFinalInitForServices(Profile* profile,
                                            bool go_off_the_record) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  ExtensionSystem::Get(profile)->Init(!go_off_the_record);
  if (!command_line.HasSwitch(switches::kDisableWebResources))
    profile->InitPromoResources();
}

void ProfileManager::DoFinalInitLogging(Profile* profile) {
  // Count number of extensions in this profile.
  int extension_count = -1;
  ExtensionService* extension_service = profile->GetExtensionService();
  if (extension_service)
    extension_count = extension_service->GetAppIds().size();

  // Log the profile size after a reasonable startup delay.
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ProfileSizeTask, profile->GetPath(), extension_count),
      base::TimeDelta::FromSeconds(112));
}

Profile* ProfileManager::CreateProfileHelper(const FilePath& path) {
  return Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
}

Profile* ProfileManager::CreateProfileAsyncHelper(const FilePath& path,
                                                  Delegate* delegate) {
  return Profile::CreateProfile(path,
                                delegate,
                                Profile::CREATE_MODE_ASYNCHRONOUS);
}

#if defined(OS_WIN)
ProfileShortcutManagerWin* ProfileManager::CreateShortcutManager() {
  return new ProfileShortcutManagerWin();
}
#endif

void ProfileManager::OnProfileCreated(Profile* profile,
                                      bool success,
                                      bool is_new_profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ProfilesInfoMap::iterator iter = profiles_info_.find(profile->GetPath());
  DCHECK(iter != profiles_info_.end());
  ProfileInfo* info = iter->second.get();

  std::vector<CreateCallback> callbacks;
  info->callbacks.swap(callbacks);

  // Invoke CREATED callback for normal profiles.
  bool go_off_the_record = ShouldGoOffTheRecord();
  if (success && !go_off_the_record)
    RunCallbacks(callbacks, profile, Profile::CREATE_STATUS_CREATED);

  // Perform initialization.
  if (success) {
    DoFinalInit(profile, go_off_the_record);
    if (go_off_the_record)
      profile = profile->GetOffTheRecordProfile();
    info->created = true;
  } else {
    profile = NULL;
    profiles_info_.erase(iter);
  }

  // Invoke CREATED callback for incognito profiles.
  if (profile && go_off_the_record)
    RunCallbacks(callbacks, profile, Profile::CREATE_STATUS_CREATED);

  // Invoke INITIALIZED or FAIL for all profiles.
  RunCallbacks(callbacks, profile,
               profile ? Profile::CREATE_STATUS_INITIALIZED :
                         Profile::CREATE_STATUS_FAIL);
}

FilePath ProfileManager::GenerateNextProfileDirectoryPath() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  DCHECK(IsMultipleProfilesEnabled());

  // Create the next profile in the next available directory slot.
  int next_directory = local_state->GetInteger(prefs::kProfilesNumCreated);
  std::string profile_name = chrome::kMultiProfileDirPrefix;
  profile_name.append(base::IntToString(next_directory));
  FilePath new_path = user_data_dir_;
#if defined(OS_WIN)
  new_path = new_path.Append(ASCIIToUTF16(profile_name));
#else
  new_path = new_path.Append(profile_name);
#endif
  local_state->SetInteger(prefs::kProfilesNumCreated, ++next_directory);
  return new_path;
}

// static
void ProfileManager::CreateMultiProfileAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();

  profile_manager->CreateProfileAsync(new_path,
                                      base::Bind(&OnOpenWindowForNewProfile));
}

// static
void ProfileManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kProfileLastUsed, "");
  prefs->RegisterIntegerPref(prefs::kProfilesNumCreated, 1);
  prefs->RegisterListPref(prefs::kProfilesLastActive);
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
#if defined(OS_WIN)
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    ProfileShortcutManagerWin* shortcut_manager = CreateShortcutManager();
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (dist && dist->CanCreateDesktopShortcuts() && shortcut_manager &&
        !command_line.HasSwitch(switches::kDisableDesktopShortcuts)) {
      profile_shortcut_manager_.reset(shortcut_manager);
      profile_info_cache_->AddObserver(profile_shortcut_manager_.get());
    }
#endif
  }
  return *profile_info_cache_.get();
}

void ProfileManager::AddProfileToCache(Profile* profile) {
  ProfileInfoCache& cache = GetProfileInfoCache();
  if (profile->GetPath().DirName() != cache.GetUserDataDir())
    return;

  if (cache.GetIndexOfProfileWithPath(profile->GetPath()) != std::string::npos)
    return;

  string16 username = UTF8ToUTF16(profile->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername));

  // Profile name and avatar are set by InitProfileUserPrefs and stored in the
  // profile. Use those values to setup the cache entry.
  string16 profile_name = UTF8ToUTF16(profile->GetPrefs()->GetString(
      prefs::kProfileName));

  size_t icon_index = profile->GetPrefs()->GetInteger(
      prefs::kProfileAvatarIndex);

  cache.AddProfileToCache(profile->GetPath(),
                          profile_name,
                          username,
                          icon_index);
}

#if defined(OS_WIN)
void ProfileManager::CreateDesktopShortcut(Profile* profile) {
  // TODO(sail): Disable creating new shortcuts for now.
  return;

  // Some distributions and tests cannot create desktop shortcuts, in which case
  // profile_shortcut_manager_ will not be set.
  if (!profile_shortcut_manager_.get())
    return;

  bool shortcut_created =
      profile->GetPrefs()->GetBoolean(prefs::kProfileShortcutCreated);
  if (!shortcut_created && GetNumberOfProfiles() > 1) {
    profile_shortcut_manager_->AddProfileShortcut(profile->GetPath());

    // We only ever create the shortcut for a profile once, so set a pref
    // reminding us to skip this in the future.
    profile->GetPrefs()->SetBoolean(prefs::kProfileShortcutCreated, true);
  }
}
#endif

void ProfileManager::InitProfileUserPrefs(Profile* profile) {
  ProfileInfoCache& cache = GetProfileInfoCache();

  if (profile->GetPath().DirName() != cache.GetUserDataDir())
    return;

  // Initialize the user preferences (name and avatar) only if the profile
  // doesn't have default preferenc values for them.
  if (HasAnyDefaultUserPrefs(profile)) {
    size_t profile_cache_index =
        cache.GetIndexOfProfileWithPath(profile->GetPath());
    // If the cache has an entry for this profile, use the cache data
    if (profile_cache_index != std::string::npos) {
      profile->GetPrefs()->SetInteger(prefs::kProfileAvatarIndex,
          cache.GetAvatarIconIndexOfProfileAtIndex(profile_cache_index));
      profile->GetPrefs()->SetString(prefs::kProfileName,
          UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_cache_index)));
    } else if (profile->GetPath() ==
               GetDefaultProfileDir(cache.GetUserDataDir())) {
      profile->GetPrefs()->SetInteger(prefs::kProfileAvatarIndex, 0);
      profile->GetPrefs()->SetString(prefs::kProfileName,
          l10n_util::GetStringUTF8(IDS_DEFAULT_PROFILE_NAME));
    } else {
      size_t icon_index = cache.ChooseAvatarIconIndexForNewProfile();
      profile->GetPrefs()->SetInteger(prefs::kProfileAvatarIndex, icon_index);
      profile->GetPrefs()->SetString(
          prefs::kProfileName,
          UTF16ToUTF8(cache.ChooseNameForNewProfile(icon_index)));
    }
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
  DCHECK(IsMultipleProfilesEnabled());

  // If we're deleting the last profile, then create a new profile in its
  // place.
  ProfileInfoCache& cache = GetProfileInfoCache();
  if (cache.GetNumberOfProfiles() == 1) {
    FilePath new_path = GenerateNextProfileDirectoryPath();

    CreateProfileAsync(new_path, base::Bind(&OnOpenWindowForNewProfile));
  }

  // Update the last used profile pref before closing browser windows. This way
  // the correct last used profile is set for any notification observers.
  PrefService* local_state = g_browser_process->local_state();
  std::string last_profile = local_state->GetString(prefs::kProfileLastUsed);
  if (profile_dir.BaseName().MaybeAsASCII() == last_profile) {
    for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
      FilePath cur_path = cache.GetPathOfProfileAtIndex(i);
      if (cur_path != profile_dir) {
        local_state->SetString(
            prefs::kProfileLastUsed, cur_path.BaseName().MaybeAsASCII());
        break;
      }
    }
  }

  // TODO(sail): Due to bug 88586 we don't delete the profile instance. Once we
  // start deleting the profile instance we need to close background apps too.
  Profile* profile = GetProfileByPath(profile_dir);
  if (profile) {
    BrowserList::CloseAllBrowsersWithProfile(profile);

    // Disable sync for doomed profile.
    if (ProfileSyncServiceFactory::GetInstance()->HasProfileSyncService(
        profile)) {
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          profile)->DisableForUser();
    }
  }

  QueueProfileDirectoryForDeletion(profile_dir);
  cache.DeleteProfileFromCache(profile_dir);

  ProfileMetrics::LogNumberOfProfiles(this,
                                      ProfileMetrics::DELETE_PROFILE_EVENT);
}

// static
bool ProfileManager::IsMultipleProfilesEnabled() {
#if defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kMultiProfiles))
    return false;
#endif
  return !ManagedMode::IsInManagedMode();
}

void ProfileManager::AutoloadProfiles() {
  ProfileInfoCache& cache = GetProfileInfoCache();
  size_t number_of_profiles = cache.GetNumberOfProfiles();
  for (size_t p = 0; p < number_of_profiles; ++p) {
    if (cache.GetBackgroundStatusOfProfileAtIndex(p)) {
      // If status is true, that profile is running background apps. By calling
      // GetProfile, we automatically cause the profile to be loaded which will
      // register it with the BackgroundModeManager.
      GetProfile(cache.GetPathOfProfileAtIndex(p));
    }
  }
  ProfileMetrics::LogNumberOfProfiles(this,
                                      ProfileMetrics::STARTUP_PROFILE_EVENT);
}

ProfileManagerWithoutInit::ProfileManagerWithoutInit(
    const FilePath& user_data_dir) : ProfileManager(user_data_dir) {
}

void ProfileManager::RegisterTestingProfile(Profile* profile,
                                            bool add_to_cache) {
  RegisterProfile(profile, true);
  if (add_to_cache) {
    InitProfileUserPrefs(profile);
    AddProfileToCache(profile);
  }
}

void ProfileManager::RunCallbacks(const std::vector<CreateCallback>& callbacks,
                                  Profile* profile,
                                  Profile::CreateStatus status) {
  for (size_t i = 0; i < callbacks.size(); ++i)
    callbacks[i].Run(profile, status);
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/profiles/profile_manager.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/sync_promo_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
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

using content::BrowserThread;

namespace {

// Profiles that should be deleted on shutdown.
std::vector<FilePath>& ProfilesToDelete() {
  CR_DEFINE_STATIC_LOCAL(std::vector<FilePath>, profiles_to_delete, ());
  return profiles_to_delete;
}

// Simple task to log the size of the current profile.
class ProfileSizeTask : public Task {
 public:
  explicit ProfileSizeTask(Profile* profile);
  virtual ~ProfileSizeTask() {}

  virtual void Run();
 private:
  FilePath path_;
  int extension_count_;
};

ProfileSizeTask::ProfileSizeTask(Profile* profile)
    : path_(profile->GetPath()), extension_count_(-1) {
  // This object should not remember the profile pointer since it should not
  // be accessed from IO thread.

  // Count number of extensions in this profile.
  ExtensionService* extension_service = profile->GetExtensionService();
  if (extension_service)
    extension_count_ = extension_service->GetAppIds().size();
}

void ProfileSizeTask::Run() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  int64 size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.HistorySize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalHistorySize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.CookiesSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.BookmarksSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.FaviconsSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.TopSitesSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.VisitedLinksSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.WebDataSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.ExtensionSize", size_MB);

  // Count number of extensions in this profile, if we know.
  if (extension_count_ != -1) {
    UMA_HISTOGRAM_COUNTS_10000("Profile.AppCount", extension_count_);

    static bool default_apps_trial_exists = base::FieldTrialList::TrialExists(
        kDefaultAppsTrial_Name);
    if (default_apps_trial_exists) {
      UMA_HISTOGRAM_COUNTS_10000(
          base::FieldTrial::MakeName("Profile.AppCount",
                                     kDefaultAppsTrial_Name),
          extension_count_);
    }
  }
}

void QueueProfileDirectoryForDeletion(const FilePath& path) {
  ProfilesToDelete().push_back(path);
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
      ProfileManager::NewWindowWithProfile(profile,
                                           BrowserInit::IS_PROCESS_STARTUP,
                                           BrowserInit::IS_FIRST_RUN);
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
void ProfileManager::NukeDeletedProfilesFromDisk() {
  for (std::vector<FilePath>::iterator it =
          ProfilesToDelete().begin();
       it != ProfilesToDelete().end();
       ++it) {
    file_util::Delete(*it, true);
  }
  ProfilesToDelete().clear();
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
      content::NotificationService::AllSources());
#endif
}

ProfileManager::~ProfileManager() {
  BrowserList::RemoveObserver(this);
#if defined(OS_WIN)
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

// static
void ProfileManager::NewWindowWithProfile(
    Profile* profile,
    BrowserInit::IsProcessStartup process_startup,
    BrowserInit::IsFirstRun is_first_run) {
  DCHECK(profile);
  Browser* browser = BrowserList::FindTabbedBrowser(profile, false);
  if (browser) {
    browser->window()->Activate();
  } else {
    UserMetrics::RecordAction(UserMetricsAction("NewWindow"));
    CommandLine command_line(CommandLine::NO_PROGRAM);
    int return_code;
    BrowserInit browser_init;
    browser_init.LaunchBrowser(command_line, profile, FilePath(),
                               process_startup, is_first_run, &return_code);
  }
}

void ProfileManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType)) {
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
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_IMPORT_FINISHED,
      content::Source<Profile>(profile),
      content::NotificationService::NoDetails());
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
  DoFinalInitForServices(profile, go_off_the_record);
  AddProfileToCache(profile);
  DoFinalInitLogging(profile);
}

void ProfileManager::DoFinalInitForServices(Profile* profile,
                                         bool go_off_the_record) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  profile->InitExtensions(!go_off_the_record);
  if (!command_line.HasSwitch(switches::kDisableWebResources))
    profile->InitPromoResources();
}

void ProfileManager::DoFinalInitLogging(Profile* profile) {
  // Log the profile size after a reasonable startup delay.
  BrowserThread::PostDelayedTask(BrowserThread::FILE, FROM_HERE,
                                 new ProfileSizeTask(profile), 112000);
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

FilePath ProfileManager::GenerateNextProfileDirectoryPath() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

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
#if defined(OS_WIN)
    profile_shortcut_manager_.reset(new ProfileShortcutManagerWin());
    profile_info_cache_->AddObserver(profile_shortcut_manager_.get());
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

  if (profile->GetPath() == GetDefaultProfileDir(cache.GetUserDataDir())) {
    cache.AddProfileToCache(
        profile->GetPath(),
        l10n_util::GetStringUTF16(IDS_DEFAULT_PROFILE_NAME), username, 0);
  } else {
    size_t icon_index = cache.ChooseAvatarIconIndexForNewProfile();
    cache.AddProfileToCache(profile->GetPath(),
                            cache.ChooseNameForNewProfile(icon_index),
                            username,
                            icon_index);
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
  // If we're deleting the last profile, then create a new profile in its
  // place.
  ProfileInfoCache& cache = GetProfileInfoCache();
  if (cache.GetNumberOfProfiles() == 1) {
    FilePath new_path = GenerateNextProfileDirectoryPath();

    // The launcher is deleted by the manager when profile creation is finished.
    NewProfileLauncher* launcher = new NewProfileLauncher();
    CreateProfileAsync(new_path, launcher);
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
  if (profile)
    BrowserList::CloseAllBrowsersWithProfile(profile);

  QueueProfileDirectoryForDeletion(profile_dir);
  cache.DeleteProfileFromCache(profile_dir);
}

// static
bool ProfileManager::IsMultipleProfilesEnabled() {
#if defined(OS_CHROMEOS)
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kMultiProfiles);
#else
  return true;
#endif
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

#if defined(OS_WIN)
void ProfileManager::RemoveProfileShortcutManagerForTesting() {
  profile_info_cache_->RemoveObserver(profile_shortcut_manager_.get());
}
#endif

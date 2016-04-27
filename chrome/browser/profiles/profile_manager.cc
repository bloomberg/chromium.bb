// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager.h"

#include <stdint.h>

#include <set>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/scoped_profile.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/startup_task_runner_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/password_manager/password_manager_setting_migrator_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/cross_device_promo.h"
#include "chrome/browser/signin/cross_device_promo_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/startup_task_runner_service.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/sync/browser/password_manager_setting_migrator_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/default_search_manager.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job.h"
#include "sync/internal_api/public/base/stop_source.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/ntp_snippets/ntp_snippets_service_factory.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#endif

using base::UserMetricsAction;
using content::BrowserThread;

namespace {

// Profiles that should be deleted on shutdown.
std::vector<base::FilePath>& ProfilesToDelete() {
  CR_DEFINE_STATIC_LOCAL(std::vector<base::FilePath>, profiles_to_delete, ());
  return profiles_to_delete;
}

int64_t ComputeFilesSize(const base::FilePath& directory,
                         const base::FilePath::StringType& pattern) {
  int64_t running_size = 0;
  base::FileEnumerator iter(directory, false, base::FileEnumerator::FILES,
                            pattern);
  while (!iter.Next().empty())
    running_size += iter.GetInfo().GetSize();
  return running_size;
}

// Simple task to log the size of the current profile.
void ProfileSizeTask(const base::FilePath& path, int enabled_app_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  const int64_t kBytesInOneMB = 1024 * 1024;

  int64_t size = ComputeFilesSize(path, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.HistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalHistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.CookiesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.BookmarksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.FaviconsSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TopSitesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.VisitedLinksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.WebDataSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.ExtensionSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Policy"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.PolicySize", size_MB);

  // Count number of enabled apps in this profile, if we know.
  if (enabled_app_count != -1)
    UMA_HISTOGRAM_COUNTS_10000("Profile.AppCount", enabled_app_count);
}

#if !defined(OS_ANDROID)
void QueueProfileDirectoryForDeletion(const base::FilePath& path) {
  ProfilesToDelete().push_back(path);
}
#endif

bool IsProfileMarkedForDeletion(const base::FilePath& profile_path) {
  return std::find(ProfilesToDelete().begin(), ProfilesToDelete().end(),
      profile_path) != ProfilesToDelete().end();
}

// Physically remove deleted profile directories from disk.
void NukeProfileFromDisk(const base::FilePath& profile_path) {
  // Delete both the profile directory and its corresponding cache.
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(profile_path, &cache_path);
  base::DeleteFile(profile_path, true);
  base::DeleteFile(cache_path, true);
}

#if defined(OS_CHROMEOS)
void CheckCryptohomeIsMounted(chromeos::DBusMethodCallStatus call_status,
                              bool is_mounted) {
  if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "IsMounted call failed.";
    return;
  }
  if (!is_mounted)
    LOG(ERROR) << "Cryptohome is not mounted.";
}

#endif

#if defined(ENABLE_EXTENSIONS)

// Returns the number of installed (and enabled) apps, excluding any component
// apps.
size_t GetEnabledAppCount(Profile* profile) {
  size_t installed_apps = 0u;
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    if ((*iter)->is_app() &&
        (*iter)->location() != extensions::Manifest::COMPONENT) {
      ++installed_apps;
    }
  }
  return installed_apps;
}

#endif  // ENABLE_EXTENSIONS

// Once a profile is loaded through LoadProfile this method is executed.
// It will then run |client_callback| with the right profile or null if it was
// unable to load it.
// It might get called more than once with different values of
// |status| but only once the profile is fully initialized will
// |client_callback| be run.
void OnProfileLoaded(
    const ProfileManager::ProfileLoadedCallback& client_callback,
    bool incognito,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_CREATED) {
    // This is an intermediate state where the profile has been created, but is
    // not yet initialized. Ignore this and wait for the next state change.
    return;
  }
  if (status != Profile::CREATE_STATUS_INITIALIZED) {
    LOG(WARNING) << "Profile not loaded correctly";
    client_callback.Run(nullptr);
    return;
  }
  DCHECK(profile);
  client_callback.Run(incognito ? profile->GetOffTheRecordProfile() : profile);
}

}  // namespace

ProfileManager::ProfileManager(const base::FilePath& user_data_dir)
    : user_data_dir_(user_data_dir),
      logged_in_(false),
#if !defined(OS_ANDROID)
      browser_list_observer_(this),
#endif
      closing_all_browsers_(false) {
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
      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
      content::NotificationService::AllSources());

  if (ProfileShortcutManager::IsFeatureEnabled() && !user_data_dir_.empty())
    profile_shortcut_manager_.reset(ProfileShortcutManager::Create(
                                    this));
}

ProfileManager::~ProfileManager() {
}

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
  for (std::vector<base::FilePath>::iterator it =
          ProfilesToDelete().begin();
       it != ProfilesToDelete().end();
       ++it) {
    NukeProfileFromDisk(*it);
  }
  ProfilesToDelete().clear();
}

// static
Profile* ProfileManager::GetLastUsedProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastUsedProfile(profile_manager->user_data_dir_);
}

// static
Profile* ProfileManager::GetLastUsedProfileAllowedByPolicy() {
  Profile* profile = GetLastUsedProfile();
  if (profile->IsGuestSession() ||
      profile->IsSystemProfile() ||
      IncognitoModePrefs::GetAvailability(profile->GetPrefs()) ==
      IncognitoModePrefs::FORCED) {
    return profile->GetOffTheRecordProfile();
  }
  return profile;
}

// static
std::vector<Profile*> ProfileManager::GetLastOpenedProfiles() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastOpenedProfiles(
      profile_manager->user_data_dir_);
}

// static
Profile* ProfileManager::GetPrimaryUserProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if defined(OS_CHROMEOS)
  if (!profile_manager->IsLoggedIn() ||
      !user_manager::UserManager::IsInitialized())
    return profile_manager->GetActiveUserOrOffTheRecordProfileFromPath(
        profile_manager->user_data_dir());
  user_manager::UserManager* manager = user_manager::UserManager::Get();
  // Note: The ProfileHelper will take care of guest profiles.
  return chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(
      manager->GetPrimaryUser());
#else
  return profile_manager->GetActiveUserOrOffTheRecordProfileFromPath(
      profile_manager->user_data_dir());
#endif
}

// static
Profile* ProfileManager::GetActiveUserProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if defined(OS_CHROMEOS)
  if (!profile_manager)
    return nullptr;

  if (profile_manager->IsLoggedIn() &&
      user_manager::UserManager::IsInitialized()) {
    user_manager::UserManager* manager = user_manager::UserManager::Get();
    const user_manager::User* user = manager->GetActiveUser();
    // To avoid an endless loop (crbug.com/334098) we have to additionally check
    // if the profile of the user was already created. If the profile was not
    // yet created we load the profile using the profile directly.
    // TODO: This should be cleaned up with the new profile manager.
    if (user && user->is_profile_created())
      return chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  }
#endif
  Profile* profile =
      profile_manager->GetActiveUserOrOffTheRecordProfileFromPath(
          profile_manager->user_data_dir());
  // |profile| could be null if the user doesn't have a profile yet and the path
  // is on a read-only volume (preventing Chrome from making a new one).
  // However, most callers of this function immediately dereference the result
  // which would lead to crashes in a variety of call sites. Assert here to
  // figure out how common this is. http://crbug.com/383019
  CHECK(profile) << profile_manager->user_data_dir().AsUTF8Unsafe();
  return profile;
}

Profile* ProfileManager::GetProfile(const base::FilePath& profile_dir) {
  TRACE_EVENT0("browser", "ProfileManager::GetProfile");

  // If the profile is already loaded (e.g., chrome.exe launched twice), just
  // return it.
  Profile* profile = GetProfileByPath(profile_dir);
  if (profile)
    return profile;
  return CreateAndInitializeProfile(profile_dir);
}

size_t ProfileManager::GetNumberOfProfiles() {
  return GetProfileAttributesStorage().GetNumberOfProfiles();
}

bool ProfileManager::LoadProfile(const std::string& profile_name,
                                 bool incognito,
                                 const ProfileLoadedCallback& callback) {
  const base::FilePath profile_path = user_data_dir().AppendASCII(profile_name);

  ProfileAttributesEntry* entry = nullptr;
  if (!GetProfileAttributesStorage().GetProfileAttributesWithPath(profile_path,
                                                                  &entry)) {
    callback.Run(nullptr);
    LOG(ERROR) << "Loading a profile path that does not exist";
    return false;
  }
  CreateProfileAsync(profile_path,
                     base::Bind(&OnProfileLoaded, callback, incognito),
                     base::string16() /* name */, std::string() /* icon_url */,
                     std::string() /* supervided_user_id */);
  return true;
}

void ProfileManager::CreateProfileAsync(
    const base::FilePath& profile_path,
    const CreateCallback& callback,
    const base::string16& name,
    const std::string& icon_url,
    const std::string& supervised_user_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("browser,startup",
               "ProfileManager::CreateProfileAsync",
               "profile_path",
               profile_path.AsUTF8Unsafe());

  // Make sure that this profile is not pending deletion.
  if (IsProfileMarkedForDeletion(profile_path)) {
    if (!callback.is_null())
      callback.Run(NULL, Profile::CREATE_STATUS_LOCAL_FAIL);
    return;
  }

  // Create the profile if needed and collect its ProfileInfo.
  ProfilesInfoMap::iterator iter = profiles_info_.find(profile_path);
  ProfileInfo* info = NULL;

  if (iter != profiles_info_.end()) {
    info = iter->second.get();
  } else {
    // Initiate asynchronous creation process.
    info = RegisterProfile(CreateProfileAsyncHelper(profile_path, this), false);
    size_t icon_index;
    DCHECK(base::IsStringASCII(icon_url));
    if (profiles::IsDefaultAvatarIconUrl(icon_url, &icon_index)) {
      // add profile to cache with user selected name and avatar
      GetProfileAttributesStorage().AddProfile(profile_path, name,
          std::string(), base::string16(), icon_index, supervised_user_id);
    }

    if (!supervised_user_id.empty()) {
      content::RecordAction(
          UserMetricsAction("ManagedMode_LocallyManagedUserCreated"));
    }

    ProfileMetrics::UpdateReportedProfilesStatistics(this);
  }

  // Call or enqueue the callback.
  if (!callback.is_null()) {
    if (iter != profiles_info_.end() && info->created) {
      Profile* profile = info->profile.get();
      // If this was the guest profile, apply settings and go OffTheRecord.
      // The system profile also needs characteristics of being off the record,
      // such as having no extensions, not writing to disk, etc.
      if (profile->IsGuestSession() || profile->IsSystemProfile()) {
        SetNonPersonalProfilePrefs(profile);
        profile = profile->GetOffTheRecordProfile();
      }
      // Profile has already been created. Run callback immediately.
      callback.Run(profile, Profile::CREATE_STATUS_INITIALIZED);
    } else {
      // Profile is either already in the process of being created, or new.
      // Add callback to the list.
      info->callbacks.push_back(callback);
    }
  }
}

bool ProfileManager::IsValidProfile(const void* profile) {
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

base::FilePath ProfileManager::GetInitialProfileDir() {
#if defined(OS_CHROMEOS)
  if (logged_in_) {
    return chromeos::ProfileHelper::Get()->GetActiveUserProfileDir();
  }
#endif
  base::FilePath relative_profile_dir;
  // TODO(mirandac): should not automatically be default profile.
  return relative_profile_dir.AppendASCII(chrome::kInitialProfile);
}

Profile* ProfileManager::GetLastUsedProfile(
    const base::FilePath& user_data_dir) {
#if defined(OS_CHROMEOS)
  // Use default login profile if user has not logged in yet.
  if (!logged_in_) {
    return GetActiveUserOrOffTheRecordProfileFromPath(user_data_dir);
  } else {
    // CrOS multi-profiles implementation is different so GetLastUsedProfile
    // has custom implementation too.
    base::FilePath profile_dir;
    // In case of multi-profiles we ignore "last used profile" preference
    // since it may refer to profile that has been in use in previous session.
    // That profile dir may not be mounted in this session so instead return
    // active profile from current session.
    profile_dir = chromeos::ProfileHelper::Get()->GetActiveUserProfileDir();

    base::FilePath profile_path(user_data_dir);
    Profile* profile = GetProfileByPath(profile_path.Append(profile_dir));
    // If we get here, it means the user has logged in but the profile has not
    // finished initializing, so treat the user as not having logged in.
    if (!profile) {
      DLOG(WARNING) << "Calling GetLastUsedProfile() before profile "
                    << "initialization is completed. Returning login profile.";
      return GetActiveUserOrOffTheRecordProfileFromPath(user_data_dir);
    }
    return profile->IsGuestSession() ? profile->GetOffTheRecordProfile() :
                                       profile;
  }
#else

  return GetProfile(GetLastUsedProfileDir(user_data_dir));
#endif
}

base::FilePath ProfileManager::GetLastUsedProfileDir(
    const base::FilePath& user_data_dir) {
  return user_data_dir.AppendASCII(GetLastUsedProfileName());
}

std::string ProfileManager::GetLastUsedProfileName() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const std::string last_used_profile_name =
      local_state->GetString(prefs::kProfileLastUsed);
  // Make sure the system profile can't be the one marked as the last one used
  // since it shouldn't get a browser.
  if (!last_used_profile_name.empty() &&
      last_used_profile_name !=
          base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe()) {
    return last_used_profile_name;
  }

  return chrome::kInitialProfile;
}

std::vector<Profile*> ProfileManager::GetLastOpenedProfiles(
    const base::FilePath& user_data_dir) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  std::vector<Profile*> to_return;
  if (local_state->HasPrefPath(prefs::kProfilesLastActive) &&
      local_state->GetList(prefs::kProfilesLastActive)) {
    // Make a copy because the list might change in the calls to GetProfile.
    std::unique_ptr<base::ListValue> profile_list(
        local_state->GetList(prefs::kProfilesLastActive)->DeepCopy());
    base::ListValue::const_iterator it;
    std::string profile;
    for (it = profile_list->begin(); it != profile_list->end(); ++it) {
      if (!(*it)->GetAsString(&profile) || profile.empty() ||
          profile == base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe()) {
        LOG(WARNING) << "Invalid entry in " << prefs::kProfilesLastActive;
        continue;
      }
      to_return.push_back(GetProfile(user_data_dir.AppendASCII(profile)));
    }
  }
  return to_return;
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

Profile* ProfileManager::GetProfileByPathInternal(
    const base::FilePath& path) const {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileByPathInternal");
  ProfileInfo* profile_info = GetProfileInfoByPath(path);
  return profile_info ? profile_info->profile.get() : nullptr;
}

Profile* ProfileManager::GetProfileByPath(const base::FilePath& path) const {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileByPath");
  ProfileInfo* profile_info = GetProfileInfoByPath(path);
  return (profile_info && profile_info->created) ? profile_info->profile.get()
                                                 : nullptr;
}

// static
base::FilePath ProfileManager::CreateMultiProfileAsync(
    const base::string16& name,
    const std::string& icon_url,
    const CreateCallback& callback,
    const std::string& supervised_user_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();

  profile_manager->CreateProfileAsync(new_path,
                                      callback,
                                      name,
                                      icon_url,
                                      supervised_user_id);
  return new_path;
}

// static
base::FilePath ProfileManager::GetGuestProfilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath guest_path = profile_manager->user_data_dir();
  return guest_path.Append(chrome::kGuestProfileDir);
}

// static
base::FilePath ProfileManager::GetSystemProfilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath system_path = profile_manager->user_data_dir();
  return system_path.Append(chrome::kSystemProfileDir);
}

base::FilePath ProfileManager::GenerateNextProfileDirectoryPath() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  DCHECK(profiles::IsMultipleProfilesEnabled());

  // Create the next profile in the next available directory slot.
  int next_directory = local_state->GetInteger(prefs::kProfilesNumCreated);
  std::string profile_name = chrome::kMultiProfileDirPrefix;
  profile_name.append(base::IntToString(next_directory));
  base::FilePath new_path = user_data_dir_;
#if defined(OS_WIN)
  new_path = new_path.Append(base::ASCIIToUTF16(profile_name));
#else
  new_path = new_path.Append(profile_name);
#endif
  local_state->SetInteger(prefs::kProfilesNumCreated, ++next_directory);
  return new_path;
}

ProfileInfoCache& ProfileManager::GetProfileInfoCache() {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileInfoCache");
  if (!profile_info_cache_) {
    profile_info_cache_.reset(new ProfileInfoCache(
        g_browser_process->local_state(), user_data_dir_));
  }
  return *profile_info_cache_.get();
}

ProfileAttributesStorage& ProfileManager::GetProfileAttributesStorage() {
  return GetProfileInfoCache();
}

ProfileShortcutManager* ProfileManager::profile_shortcut_manager() {
  return profile_shortcut_manager_.get();
}

#if !defined(OS_ANDROID)
void ProfileManager::ScheduleProfileForDeletion(
    const base::FilePath& profile_dir,
    const CreateCallback& callback) {
  DCHECK(profiles::IsMultipleProfilesEnabled());

  // Cancel all in-progress downloads before deleting the profile to prevent a
  // "Do you want to exit Google Chrome and cancel the downloads?" prompt
  // (crbug.com/336725).
  Profile* profile = GetProfileByPath(profile_dir);
  if (profile) {
    DownloadService* service =
        DownloadServiceFactory::GetForBrowserContext(profile);
    service->CancelDownloads();
  }

  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  // If we're deleting the last (non-legacy-supervised) profile, then create a
  // new profile in its place.
  base::FilePath last_non_supervised_profile_path;
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath cur_path = entry->GetPath();
    // Make sure that this profile is not pending deletion, and is not
    // legacy-supervised.
    if (cur_path != profile_dir &&
        !entry->IsLegacySupervised() &&
        !IsProfileMarkedForDeletion(cur_path)) {
      last_non_supervised_profile_path = cur_path;
      break;
    }
  }

  if (last_non_supervised_profile_path.empty()) {
    std::string new_avatar_url;
    base::string16 new_profile_name;

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    int avatar_index = profiles::GetPlaceholderAvatarIndex();
    new_avatar_url = profiles::GetDefaultAvatarIconUrl(avatar_index);
    new_profile_name = storage.ChooseNameForNewProfile(avatar_index);
#endif

    base::FilePath new_path(GenerateNextProfileDirectoryPath());
    CreateProfileAsync(new_path,
                       base::Bind(&ProfileManager::OnNewActiveProfileLoaded,
                                  base::Unretained(this),
                                  profile_dir,
                                  new_path,
                                  callback),
                       new_profile_name,
                       new_avatar_url,
                       std::string());

    ProfileMetrics::LogProfileAddNewUser(
        ProfileMetrics::ADD_NEW_USER_LAST_DELETED);
    return;
  }

#if defined(OS_MACOSX)
  // On the Mac, the browser process is not killed when all browser windows are
  // closed, so just in case we are deleting the active profile, and no other
  // profile has been loaded, we must pre-load a next one.
  const base::FilePath last_used_profile =
      GetLastUsedProfileDir(user_data_dir_);
  if (last_used_profile == profile_dir ||
      last_used_profile == GetGuestProfilePath()) {
    CreateProfileAsync(last_non_supervised_profile_path,
                       base::Bind(&ProfileManager::OnNewActiveProfileLoaded,
                                  base::Unretained(this),
                                  profile_dir,
                                  last_non_supervised_profile_path,
                                  callback),
                       base::string16(),
                       std::string(),
                       std::string());
    return;
  }
#endif  // defined(OS_MACOSX)

  FinishDeletingProfile(profile_dir, last_non_supervised_profile_path);
}
#endif  // !defined(OS_ANDROID)

void ProfileManager::AutoloadProfiles() {
  // If running in the background is disabled for the browser, do not autoload
  // any profiles.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  if (!local_state->HasPrefPath(prefs::kBackgroundModeEnabled) ||
      !local_state->GetBoolean(prefs::kBackgroundModeEnabled)) {
    return;
  }

  std::vector<ProfileAttributesEntry*> entries =
      GetProfileAttributesStorage().GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (entry->GetBackgroundStatus()) {
      // If status is true, that profile is running background apps. By calling
      // GetProfile, we automatically cause the profile to be loaded which will
      // register it with the BackgroundModeManager.
      GetProfile(entry->GetPath());
    }
  }
}

void ProfileManager::CleanUpEphemeralProfiles() {
  const std::string last_used_profile = GetLastUsedProfileName();

  bool last_active_profile_deleted = false;
  base::FilePath new_profile_path;
  std::vector<base::FilePath> profiles_to_delete;
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath profile_path = entry->GetPath();
    if (entry->IsEphemeral()) {
      profiles_to_delete.push_back(profile_path);
      if (profile_path.BaseName().MaybeAsASCII() == last_used_profile)
        last_active_profile_deleted = true;
    } else if (new_profile_path.empty()) {
      new_profile_path = profile_path;
    }
  }

  // If the last active profile was ephemeral, set a new one.
  if (last_active_profile_deleted) {
    if (new_profile_path.empty())
      new_profile_path = GenerateNextProfileDirectoryPath();

    profiles::SetLastUsedProfile(new_profile_path.BaseName().MaybeAsASCII());
  }

  // This uses a separate loop, because deleting the profile from the
  // ProfileInfoCache will modify indices.
  for (const base::FilePath& profile_path : profiles_to_delete) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&NukeProfileFromDisk, profile_path));

    storage.RemoveProfile(profile_path);
  }
}

void ProfileManager::InitProfileUserPrefs(Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::InitProfileUserPrefs");
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();

  if (profile->GetPath().DirName() != user_data_dir()) {
    UMA_HISTOGRAM_BOOLEAN("Profile.InitProfileUserPrefs.OutsideUserDir", true);
    return;
  }

  size_t avatar_index;
  std::string profile_name;
  std::string supervised_user_id;
  if (profile->IsGuestSession()) {
    profile_name = l10n_util::GetStringUTF8(IDS_PROFILES_GUEST_PROFILE_NAME);
    avatar_index = 0;
  } else {
    ProfileAttributesEntry* entry;
    bool has_entry = storage.GetProfileAttributesWithPath(profile->GetPath(),
                                                          &entry);
    // If the profile attributes storage has an entry for this profile, use the
    // data in the profile attributes storage.
    if (has_entry) {
      avatar_index = entry->GetAvatarIconIndex();
      profile_name = base::UTF16ToUTF8(entry->GetName());
      supervised_user_id = entry->GetSupervisedUserId();
    } else if (profile->GetPath() ==
                   profiles::GetDefaultProfileDir(user_data_dir())) {
      avatar_index = profiles::GetPlaceholderAvatarIndex();
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
      profile_name =
          base::UTF16ToUTF8(storage.ChooseNameForNewProfile(avatar_index));
#else
      profile_name = l10n_util::GetStringUTF8(IDS_DEFAULT_PROFILE_NAME);
#endif
    } else {
      avatar_index = storage.ChooseAvatarIconIndexForNewProfile();
      profile_name =
          base::UTF16ToUTF8(storage.ChooseNameForNewProfile(avatar_index));
    }
  }

  if (!profile->GetPrefs()->HasPrefPath(prefs::kProfileAvatarIndex))
    profile->GetPrefs()->SetInteger(prefs::kProfileAvatarIndex, avatar_index);

  if (!profile->GetPrefs()->HasPrefPath(prefs::kProfileName))
    profile->GetPrefs()->SetString(prefs::kProfileName, profile_name);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool force_supervised_user_id =
#if defined(OS_CHROMEOS)
      g_browser_process->platform_part()
              ->profile_helper()
              ->GetSigninProfileDir() != profile->GetPath() &&
#endif
      command_line->HasSwitch(switches::kSupervisedUserId);
  if (force_supervised_user_id) {
    supervised_user_id =
        command_line->GetSwitchValueASCII(switches::kSupervisedUserId);
  }
  if (force_supervised_user_id ||
      !profile->GetPrefs()->HasPrefPath(prefs::kSupervisedUserId)) {
    profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                   supervised_user_id);
  }
}

void ProfileManager::RegisterTestingProfile(Profile* profile,
                                            bool add_to_storage,
                                            bool start_deferred_task_runners) {
  RegisterProfile(profile, true);
  if (add_to_storage) {
    InitProfileUserPrefs(profile);
    AddProfileToStorage(profile);
  }
  if (start_deferred_task_runners) {
    StartupTaskRunnerServiceFactory::GetForProfile(profile)->
        StartDeferredTaskRunners();
  }
}

void ProfileManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
    logged_in_ = true;

    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType)) {
      // If we don't have a mounted profile directory we're in trouble.
      // TODO(davemoore) Once we have better api this check should ensure that
      // our profile directory is the one that's mounted, and that it's mounted
      // as the current user.
      chromeos::DBusThreadManager::Get()->GetCryptohomeClient()->IsMounted(
          base::Bind(&CheckCryptohomeIsMounted));

      // Confirm that we hadn't loaded the new profile previously.
      base::FilePath default_profile_dir = user_data_dir_.Append(
          GetInitialProfileDir());
      CHECK(!GetProfileByPathInternal(default_profile_dir))
          << "The default profile was loaded before we mounted the cryptohome.";
    }
    return;
  }
#endif
  bool save_active_profiles = false;
  switch (type) {
    case chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST: {
      // Ignore any browsers closing from now on.
      closing_all_browsers_ = true;
      save_active_profiles = true;
      break;
    }
    case chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED: {
      // This will cancel the shutdown process, so the active profiles are
      // tracked again. Also, as the active profiles may have changed (i.e. if
      // some windows were closed) we save the current list of active profiles
      // again.
      closing_all_browsers_ = false;
      save_active_profiles = true;
      break;
    }
    case chrome::NOTIFICATION_BROWSER_OPENED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      DCHECK(browser);
      Profile* profile = browser->profile();
      DCHECK(profile);
      bool is_ephemeral =
          profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles);
      if (!profile->IsOffTheRecord() && !is_ephemeral &&
          ++browser_counts_[profile] == 1) {
        active_profiles_.push_back(profile);
        save_active_profiles = true;
      }
      // If browsers are opening, we can't be closing all the browsers. This
      // can happen if the application was exited, but background mode or
      // packaged apps prevented the process from shutting down, and then
      // a new browser window was opened.
      closing_all_browsers_ = false;
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
        save_active_profiles = !closing_all_browsers_;
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }

  if (save_active_profiles) {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    ListPrefUpdate update(local_state, prefs::kProfilesLastActive);
    base::ListValue* profile_list = update.Get();

    profile_list->Clear();

    // crbug.com/120112 -> several non-incognito profiles might have the same
    // GetPath().BaseName(). In that case, we cannot restore both
    // profiles. Include each base name only once in the last active profile
    // list.
    std::set<std::string> profile_paths;
    std::vector<Profile*>::const_iterator it;
    for (it = active_profiles_.begin(); it != active_profiles_.end(); ++it) {
      std::string profile_path = (*it)->GetPath().BaseName().MaybeAsASCII();
      // Some profiles might become ephemeral after they are created.
      // Don't persist the System Profile as one of the last actives, it should
      // never get a browser.
      if (!(*it)->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles) &&
          profile_paths.find(profile_path) == profile_paths.end() &&
          profile_path !=
              base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe()) {
        profile_paths.insert(profile_path);
        profile_list->Append(new base::StringValue(profile_path));
      }
    }
  }
}

void ProfileManager::OnProfileCreated(Profile* profile,
                                      bool success,
                                      bool is_new_profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfilesInfoMap::iterator iter = profiles_info_.find(profile->GetPath());
  DCHECK(iter != profiles_info_.end());
  ProfileInfo* info = iter->second.get();

  std::vector<CreateCallback> callbacks;
  info->callbacks.swap(callbacks);

  // Invoke CREATED callback for normal profiles.
  bool go_off_the_record = ShouldGoOffTheRecord(profile);
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
    // TODO(yiyaoliu): This is temporary, remove it after it's not used.
    UMA_HISTOGRAM_COUNTS_100("UMA.ProfilesCount.AfterErase",
                             profiles_info_.size());
  }

  if (profile) {
    // If this was the guest or system profile, finish setting its special
    // status.
    if (profile->IsGuestSession() || profile->IsSystemProfile())
      SetNonPersonalProfilePrefs(profile);

    // Invoke CREATED callback for incognito profiles.
    if (go_off_the_record)
      RunCallbacks(callbacks, profile, Profile::CREATE_STATUS_CREATED);
  }

  // Invoke INITIALIZED or FAIL for all profiles.
  RunCallbacks(callbacks, profile,
               profile ? Profile::CREATE_STATUS_INITIALIZED :
                         Profile::CREATE_STATUS_LOCAL_FAIL);
}

void ProfileManager::DoFinalInit(Profile* profile, bool go_off_the_record) {
  TRACE_EVENT0("browser", "ProfileManager::DoFinalInit");
  TRACK_SCOPED_REGION("Startup", "ProfileManager::DoFinalInit");

  DoFinalInitForServices(profile, go_off_the_record);
  AddProfileToStorage(profile);
  DoFinalInitLogging(profile);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile),
      content::NotificationService::NoDetails());

#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
  // Record statistics to ProfileInfoCache if statistics were not recorded
  // during shutdown, i.e. the last shutdown was a system shutdown or a crash.
  if (!profile->IsGuestSession() && !profile->IsSystemProfile() &&
      !profile->IsNewProfile() && !go_off_the_record &&
      profile->GetLastSessionExitType() != Profile::EXIT_NORMAL) {
    ProfileStatisticsFactory::GetForProfile(profile)->GatherStatistics(
        profiles::ProfileStatisticsCallback());
  }
#endif
}

void ProfileManager::DoFinalInitForServices(Profile* profile,
                                            bool go_off_the_record) {
  TRACE_EVENT0("browser", "ProfileManager::DoFinalInitForServices");
  TRACK_SCOPED_REGION("Startup", "ProfileManager::DoFinalInitForServices");

#if defined(ENABLE_EXTENSIONS)
  // Ensure that the HostContentSettingsMap has been created before the
  // ExtensionSystem is initialized otherwise the ExtensionSystem will be
  // registered twice
  HostContentSettingsMap* content_settings_map =
    HostContentSettingsMapFactory::GetForProfile(profile);

  extensions::ExtensionSystem::Get(profile)->InitForRegularProfile(
      !go_off_the_record);
  // During tests, when |profile| is an instance of TestingProfile,
  // ExtensionSystem might not create an ExtensionService.
  // This block is duplicated in the HostContentSettingsMapFactory
  // ::BuildServiceInstanceFor method, it should be called once when both the
  // HostContentSettingsMap and the extension_service are set up.
  if (extensions::ExtensionSystem::Get(profile)->extension_service()) {
    extensions::ExtensionSystem::Get(profile)->extension_service()->
        RegisterContentSettings(content_settings_map);
  }
  // Set the block extensions bit on the ExtensionService. There likely are no
  // blockable extensions to block.
  ProfileAttributesEntry* entry;
  bool has_entry = GetProfileAttributesStorage().
                       GetProfileAttributesWithPath(profile->GetPath(), &entry);
  if (has_entry && entry->IsSigninRequired()) {
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->BlockAllExtensions();
  }

#endif
#if defined(ENABLE_SUPERVISED_USERS)
  // Initialization needs to happen after extension system initialization (for
  // extension::ManagementPolicy) and InitProfileUserPrefs (for setting the
  // initializing the supervised flag if necessary).
  ChildAccountServiceFactory::GetForProfile(profile)->Init();
  SupervisedUserServiceFactory::GetForProfile(profile)->Init();
#endif
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // If the lock enabled algorithm changed, update this profile's lock status.
  // This depends on services which shouldn't be initialized until
  // DoFinalInitForServices.
  if (switches::IsNewProfileManagement())
    profiles::UpdateIsProfileLockEnabledIfNeeded(profile);
#endif
  // Start the deferred task runners once the profile is loaded.
  StartupTaskRunnerServiceFactory::GetForProfile(profile)->
      StartDeferredTaskRunners();

  // Activate data reduction proxy. This creates a request context and makes a
  // URL request to check if the data reduction proxy server is reachable.
  DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile)->
      MaybeActivateDataReductionProxy(true);

  GaiaCookieManagerServiceFactory::GetForProfile(profile)->Init();
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile);
  // Chrome OS login and guest profiles do not support invalidation. This is
  // fine as they do not have GAIA credentials anyway. http://crbug.com/358169
  invalidation::InvalidationService* invalidation_service =
      invalidation_provider ? invalidation_provider->GetInvalidationService()
                            : nullptr;
  AccountFetcherServiceFactory::GetForProfile(profile)
      ->SetupInvalidationsOnProfileLoad(invalidation_service);
  AccountReconcilorFactory::GetForProfile(profile);

  // Service is responsible for migration of the legacy password manager
  // preference which controls behaviour of the Chrome to the new preference
  // which controls password management behaviour on Chrome and Android. After
  // migration will be performed for all users it's planned to remove the
  // migration code, rough time estimates are Q1 2016.
  PasswordManagerSettingMigratorServiceFactory::GetForProfile(profile)
      ->InitializeMigration(ProfileSyncServiceFactory::GetForProfile(profile));

#if defined(OS_ANDROID)
  // Service is responsible for fetching content snippets for the NTP.
  // Note: Create the service even if the feature is disabled, so that any
  // remaining tasks will be cleaned up.
  NTPSnippetsServiceFactory::GetForProfile(profile)->Init(
      base::FeatureList::IsEnabled(chrome::android::kNTPSnippetsFeature));
#endif
}

void ProfileManager::DoFinalInitLogging(Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::DoFinalInitLogging");
  // Count number of extensions in this profile.
  int enabled_app_count = -1;
#if defined(ENABLE_EXTENSIONS)
  enabled_app_count = GetEnabledAppCount(profile);
#endif

  // Log the profile size after a reasonable startup delay.
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ProfileSizeTask, profile->GetPath(), enabled_app_count),
      base::TimeDelta::FromSeconds(112));
}

Profile* ProfileManager::CreateProfileHelper(const base::FilePath& path) {
  TRACE_EVENT0("browser", "ProfileManager::CreateProfileHelper");
  SCOPED_UMA_HISTOGRAM_TIMER("Profile.CreateProfileHelperTime");
  TRACK_SCOPED_REGION("Startup", "ProfileManager::CreateProfileHelper");

  return Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
}

Profile* ProfileManager::CreateProfileAsyncHelper(const base::FilePath& path,
                                                  Delegate* delegate) {
  return Profile::CreateProfile(path,
                                delegate,
                                Profile::CREATE_MODE_ASYNCHRONOUS);
}

Profile* ProfileManager::GetActiveUserOrOffTheRecordProfileFromPath(
    const base::FilePath& user_data_dir) {
#if defined(OS_CHROMEOS)
  base::FilePath default_profile_dir(user_data_dir);
  if (!logged_in_) {
    default_profile_dir = profiles::GetDefaultProfileDir(user_data_dir);
    Profile* profile = GetProfile(default_profile_dir);
    // For cros, return the OTR profile so we never accidentally keep
    // user data in an unencrypted profile. But doing this makes
    // many of the browser and ui tests fail. We do return the OTR profile
    // if the login-profile switch is passed so that we can test this.
    if (ShouldGoOffTheRecord(profile))
      return profile->GetOffTheRecordProfile();
    DCHECK(!user_manager::UserManager::Get()->IsLoggedInAsGuest());
    return profile;
  }

  default_profile_dir = default_profile_dir.Append(GetInitialProfileDir());
  ProfileInfo* profile_info = GetProfileInfoByPath(default_profile_dir);
  // Fallback to default off-the-record profile, if user profile has not fully
  // loaded yet.
  if (profile_info && !profile_info->created)
    default_profile_dir = profiles::GetDefaultProfileDir(user_data_dir);

  Profile* profile = GetProfile(default_profile_dir);
  // Some unit tests didn't initialize the UserManager.
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsGuest())
    return profile->GetOffTheRecordProfile();
  return profile;
#else
  base::FilePath default_profile_dir(user_data_dir);
  default_profile_dir = default_profile_dir.Append(GetInitialProfileDir());
  return GetProfile(default_profile_dir);
#endif
}

bool ProfileManager::AddProfile(Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::AddProfile");
  TRACK_SCOPED_REGION("Startup", "ProfileManager::AddProfile");

  DCHECK(profile);

  // Make sure that we're not loading a profile with the same ID as a profile
  // that's already loaded.
  if (GetProfileByPathInternal(profile->GetPath())) {
    NOTREACHED() << "Attempted to add profile with the same path (" <<
                    profile->GetPath().value() <<
                    ") as an already-loaded profile.";
    return false;
  }

  RegisterProfile(profile, true);
  InitProfileUserPrefs(profile);
  DoFinalInit(profile, ShouldGoOffTheRecord(profile));
  return true;
}

Profile* ProfileManager::CreateAndInitializeProfile(
    const base::FilePath& profile_dir) {
  TRACE_EVENT0("browser", "ProfileManager::CreateAndInitializeProfile");
  TRACK_SCOPED_REGION(
      "Startup", "ProfileManager::CreateAndInitializeProfile");
  SCOPED_UMA_HISTOGRAM_LONG_TIMER("Profile.CreateAndInitializeProfile");

  // CHECK that we are not trying to load the same profile twice, to prevent
  // profile corruption. Note that this check also covers the case when we have
  // already started loading the profile but it is not fully initialized yet,
  // which would make Bad Things happen if we returned it.
  CHECK(!GetProfileByPathInternal(profile_dir));
  Profile* profile = CreateProfileHelper(profile_dir);
  DCHECK(profile);
  if (profile) {
    bool result = AddProfile(profile);
    DCHECK(result);
  }
  return profile;
}

#if !defined(OS_ANDROID)
void ProfileManager::FinishDeletingProfile(
    const base::FilePath& profile_dir,
    const base::FilePath& new_active_profile_dir) {
  // Update the last used profile pref before closing browser windows. This
  // way the correct last used profile is set for any notification observers.
  profiles::SetLastUsedProfile(
      new_active_profile_dir.BaseName().MaybeAsASCII());

  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  // TODO(sail): Due to bug 88586 we don't delete the profile instance. Once we
  // start deleting the profile instance we need to close background apps too.
  Profile* profile = GetProfileByPath(profile_dir);

  if (profile) {
    // TODO: Migrate additional code in this block to observe this notification
    // instead of being implemented here.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_DESTRUCTION_STARTED,
        content::Source<Profile>(profile),
        content::NotificationService::NoDetails());

    // By this point, all in-progress downloads for the profile being deleted
    // must have been canceled (crbug.com/336725).
    DCHECK(DownloadServiceFactory::GetForBrowserContext(profile)->
               NonMaliciousDownloadCount() == 0);
    BrowserList::CloseAllBrowsersWithProfile(profile);

    // Disable sync for doomed profile.
    if (ProfileSyncServiceFactory::GetInstance()->HasProfileSyncService(
            profile)) {
      ProfileSyncService* sync_service =
          ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
      if (sync_service->IsSyncRequested()) {
        // Record sync stopped by profile destruction if it was on before.
        UMA_HISTOGRAM_ENUMERATION("Sync.StopSource",
                                  syncer::PROFILE_DESTRUCTION,
                                  syncer::STOP_SOURCE_LIMIT);
      }
      // Ensure data is cleared even if sync was already off.
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          profile)->RequestStop(ProfileSyncService::CLEAR_DATA);
    }

    ProfileAttributesEntry* entry;
    bool has_entry = storage.GetProfileAttributesWithPath(profile_dir, &entry);
    DCHECK(has_entry);
    ProfileMetrics::LogProfileDelete(entry->IsAuthenticated());
    // Some platforms store passwords in keychains. They should be removed.
    scoped_refptr<password_manager::PasswordStore> password_store =
        PasswordStoreFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS).get();
    if (password_store.get()) {
      password_store->RemoveLoginsCreatedBetween(
          base::Time(), base::Time::Max(), base::Closure());
    }

    // The Profile Data doesn't get wiped until Chrome closes. Since we promised
    // that the user's data would be removed, do so immediately.
    profiles::RemoveBrowsingDataForProfile(profile_dir);
  } else {
    // It is safe to delete a not yet loaded Profile from disk.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&NukeProfileFromDisk, profile_dir));
  }

  // Queue even a profile that was nuked so it will be MarkedForDeletion and so
  // CreateProfileAsync can't create it.
  QueueProfileDirectoryForDeletion(profile_dir);
  storage.RemoveProfile(profile_dir);
  ProfileMetrics::UpdateReportedProfilesStatistics(this);
}
#endif  // !defined(OS_ANDROID)

ProfileManager::ProfileInfo* ProfileManager::RegisterProfile(
    Profile* profile,
    bool created) {
  TRACE_EVENT0("browser", "ProfileManager::RegisterProfile");
  ProfileInfo* info = new ProfileInfo(profile, created);
  profiles_info_.insert(
      std::make_pair(profile->GetPath(), linked_ptr<ProfileInfo>(info)));
  return info;
}

ProfileManager::ProfileInfo* ProfileManager::GetProfileInfoByPath(
    const base::FilePath& path) const {
  ProfilesInfoMap::const_iterator iter = profiles_info_.find(path);
  return (iter == profiles_info_.end()) ? NULL : iter->second.get();
}

void ProfileManager::AddProfileToStorage(Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::AddProfileToCache");
  if (profile->IsGuestSession() || profile->IsSystemProfile())
    return;
  if (profile->GetPath().DirName() != user_data_dir()) {
    UMA_HISTOGRAM_BOOLEAN("Profile.GetProfileInfoPath.OutsideUserDir", true);
    return;
  }


  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile);
  AccountInfo account_info = account_tracker->GetAccountInfo(
      signin_manager->GetAuthenticatedAccountId());
  base::string16 username = base::UTF8ToUTF16(account_info.email);

  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  // |entry| and |has_entry| below are put inside a pair of brackets for
  // scoping, to avoid potential clashes of variable names.
  {
    ProfileAttributesEntry* entry;
    bool has_entry = storage.GetProfileAttributesWithPath(profile->GetPath(),
                                                          &entry);
    if (has_entry) {
      // The ProfileAttributesStorage's info must match the Signin Manager.
      entry->SetAuthInfo(account_info.gaia, username);
      return;
    }
  }

  // Profile name and avatar are set by InitProfileUserPrefs and stored in the
  // profile. Use those values to setup the entry in profile attributes storage.
  base::string16 profile_name =
      base::UTF8ToUTF16(profile->GetPrefs()->GetString(prefs::kProfileName));

  size_t icon_index =
      profile->GetPrefs()->GetInteger(prefs::kProfileAvatarIndex);

  std::string supervised_user_id =
      profile->GetPrefs()->GetString(prefs::kSupervisedUserId);

  storage.AddProfile(profile->GetPath(), profile_name, account_info.gaia,
                     username, icon_index, supervised_user_id);

  if (profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles)) {
    ProfileAttributesEntry* entry;
    bool has_entry = storage.GetProfileAttributesWithPath(profile->GetPath(),
                                                          &entry);
    DCHECK(has_entry);
    entry->SetIsEphemeral(true);
  }
}

void ProfileManager::SetNonPersonalProfilePrefs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kSigninAllowed, false);
  prefs->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  prefs->SetBoolean(bookmarks::prefs::kShowBookmarkBar, false);
  prefs->ClearPref(DefaultSearchManager::kDefaultSearchProviderDataPrefName);
}

bool ProfileManager::ShouldGoOffTheRecord(Profile* profile) {
#if defined(OS_CHROMEOS)
  if (profile->GetPath().BaseName().value() == chrome::kInitialProfile) {
    return true;
  }
#endif
  return profile->IsGuestSession() || profile->IsSystemProfile();
}

void ProfileManager::RunCallbacks(const std::vector<CreateCallback>& callbacks,
                                  Profile* profile,
                                  Profile::CreateStatus status) {
  for (size_t i = 0; i < callbacks.size(); ++i)
    callbacks[i].Run(profile, status);
}

ProfileManager::ProfileInfo::ProfileInfo(
    Profile* profile,
    bool created)
    : profile(profile),
      created(created) {
}

ProfileManager::ProfileInfo::~ProfileInfo() {
  ProfileDestroyer::DestroyProfileWhenAppropriate(profile.release());
}

#if !defined(OS_ANDROID)
void ProfileManager::UpdateLastUser(Profile* last_active) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Only keep track of profiles that we are managing; tests may create others.
  // Also never consider the SystemProfile as "active".
  if (profiles_info_.find(last_active->GetPath()) != profiles_info_.end() &&
      !last_active->IsSystemProfile()) {
    std::string profile_path_base =
        last_active->GetPath().BaseName().MaybeAsASCII();
    if (profile_path_base != GetLastUsedProfileName())
      profiles::SetLastUsedProfile(profile_path_base);

    ProfileAttributesEntry* entry;
    if (GetProfileAttributesStorage().
            GetProfileAttributesWithPath(last_active->GetPath(), &entry)) {
#if !defined(OS_CHROMEOS)
      // Incognito Profiles don't have ProfileKeyedServices.
      if (!last_active->IsOffTheRecord()) {
        CrossDevicePromoFactory::GetForProfile(last_active)->
            MaybeBrowsingSessionStarted(entry->GetActiveTime());
      }
#endif
      entry->SetActiveTimeToNow();
    }
  }
}

ProfileManager::BrowserListObserver::BrowserListObserver(
    ProfileManager* manager)
    : profile_manager_(manager) {
  BrowserList::AddObserver(this);
}

ProfileManager::BrowserListObserver::~BrowserListObserver() {
  BrowserList::RemoveObserver(this);
}

void ProfileManager::BrowserListObserver::OnBrowserAdded(
    Browser* browser) {}

void ProfileManager::BrowserListObserver::OnBrowserRemoved(
    Browser* browser) {
  Profile* profile = browser->profile();
  Profile* original_profile = profile->GetOriginalProfile();
  // Do nothing if the closed window is not the last window of the same profile.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() == original_profile)
      return;
  }

  base::FilePath path = profile->GetPath();
  if (IsProfileMarkedForDeletion(path)) {
    // Do nothing if the profile is already being deleted.
  } else if (profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles)) {
    // Delete if the profile is an ephemeral profile.
    g_browser_process->profile_manager()->ScheduleProfileForDeletion(
        path, ProfileManager::CreateCallback());
  } else {
#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
    // Gather statistics and store into ProfileInfoCache. For incognito profile
    // we gather the statistics of its parent profile instead, because a window
    // of the parent profile was open.
    if (!profile->IsSystemProfile()) {
      ProfileStatisticsFactory::GetForProfile(original_profile)->
          GatherStatistics(profiles::ProfileStatisticsCallback());
    }
#endif
  }
}

void ProfileManager::BrowserListObserver::OnBrowserSetLastActive(
    Browser* browser) {
  // If all browsers are being closed (e.g. the user is in the process of
  // shutting down), this event will be fired after each browser is
  // closed. This does not represent a user intention to change the active
  // browser so is not handled here.
  if (profile_manager_->closing_all_browsers_)
    return;

  Profile* last_active = browser->profile();

  // Don't remember ephemeral profiles as last because they are not going to
  // persist after restart.
  if (last_active->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles))
    return;

  profile_manager_->UpdateLastUser(last_active);
}

void ProfileManager::OnNewActiveProfileLoaded(
    const base::FilePath& profile_to_delete_path,
    const base::FilePath& new_active_profile_path,
    const CreateCallback& original_callback,
    Profile* loaded_profile,
    Profile::CreateStatus status) {
  DCHECK(status != Profile::CREATE_STATUS_LOCAL_FAIL &&
         status != Profile::CREATE_STATUS_REMOTE_FAIL);

  // Only run the code if the profile initialization has finished completely.
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  if (IsProfileMarkedForDeletion(new_active_profile_path)) {
    // If the profile we tried to load as the next active profile has been
    // deleted, then retry deleting this profile to redo the logic to load
    // the next available profile.
    ScheduleProfileForDeletion(profile_to_delete_path, original_callback);
    return;
  }

  FinishDeletingProfile(profile_to_delete_path, new_active_profile_path);
  if (!original_callback.is_null())
    original_callback.Run(loaded_profile, status);
}
#endif  // !defined(OS_ANDROID)

ProfileManagerWithoutInit::ProfileManagerWithoutInit(
    const base::FilePath& user_data_dir) : ProfileManager(user_data_dir) {
}

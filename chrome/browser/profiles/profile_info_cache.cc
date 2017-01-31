// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_avatar_downloader.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

using content::BrowserThread;

namespace {

const char kNameKey[] = "name";
const char kShortcutNameKey[] = "shortcut_name";
const char kGAIANameKey[] = "gaia_name";
const char kGAIAGivenNameKey[] = "gaia_given_name";
const char kGAIAIdKey[] = "gaia_id";
const char kUserNameKey[] = "user_name";
const char kIsUsingDefaultNameKey[] = "is_using_default_name";
const char kIsUsingDefaultAvatarKey[] = "is_using_default_avatar";
const char kAvatarIconKey[] = "avatar_icon";
const char kAuthCredentialsKey[] = "local_auth_credentials";
const char kPasswordTokenKey[] = "gaia_password_token";
const char kUseGAIAPictureKey[] = "use_gaia_picture";
const char kBackgroundAppsKey[] = "background_apps";
const char kGAIAPictureFileNameKey[] = "gaia_picture_file_name";
const char kIsOmittedFromProfileListKey[] = "is_omitted_from_profile_list";
const char kSigninRequiredKey[] = "signin_required";
const char kSupervisedUserId[] = "managed_user_id";
const char kProfileIsEphemeral[] = "is_ephemeral";
const char kActiveTimeKey[] = "active_time";
const char kIsAuthErrorKey[] = "is_auth_error";
const char kStatsBrowsingHistoryKey[] = "stats_browsing_history";
const char kStatsPasswordsKey[] = "stats_passwords";
const char kStatsBookmarksKey[] = "stats_bookmarks";
const char kStatsSettingsKey[] = "stats_settings";

typedef std::vector<unsigned char> ImageData;

// Writes |data| to disk and takes ownership of the pointer. On successful
// completion, it runs |callback|.
void SaveBitmap(std::unique_ptr<ImageData> data,
                const base::FilePath& image_path,
                const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // Make sure the destination directory exists.
  base::FilePath dir = image_path.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create parent directory.";
    return;
  }

  if (base::WriteFile(image_path, reinterpret_cast<char*>(&(*data)[0]),
                      data->size()) == -1) {
    LOG(ERROR) << "Failed to save image to file.";
    return;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

// Reads a PNG from disk and decodes it. If the bitmap was successfully read
// from disk the then |out_image| will contain the bitmap image, otherwise it
// will be NULL.
void ReadBitmap(const base::FilePath& image_path,
                gfx::Image** out_image) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  *out_image = NULL;

  // If the path doesn't exist, don't even try reading it.
  if (!base::PathExists(image_path))
    return;

  std::string image_data;
  if (!base::ReadFileToString(image_path, &image_data)) {
    LOG(ERROR) << "Failed to read PNG file from disk.";
    return;
  }

  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
      base::RefCountedString::TakeString(&image_data));
  if (image.IsEmpty()) {
    LOG(ERROR) << "Failed to decode PNG file.";
    return;
  }

  *out_image = new gfx::Image(image);
}

void RunCallbackIfFileMissing(const base::FilePath& file_path,
                              const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!base::PathExists(file_path))
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

void DeleteBitmap(const base::FilePath& image_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::DeleteFile(image_path, false);
}

}  // namespace

ProfileInfoCache::ProfileInfoCache(PrefService* prefs,
                                   const base::FilePath& user_data_dir)
    : ProfileAttributesStorage(prefs, user_data_dir),
      disable_avatar_download_for_testing_(false) {
  // Populate the cache
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  for (base::DictionaryValue::Iterator it(*cache);
       !it.IsAtEnd(); it.Advance()) {
    base::DictionaryValue* info = NULL;
    cache->GetDictionaryWithoutPathExpansion(it.key(), &info);
    base::string16 name;
    info->GetString(kNameKey, &name);
    sorted_keys_.insert(FindPositionForProfile(it.key(), name), it.key());
    profile_attributes_entries_[user_data_dir_.AppendASCII(it.key()).value()] =
        std::unique_ptr<ProfileAttributesEntry>(nullptr);

    bool using_default_name;
    if (!info->GetBoolean(kIsUsingDefaultNameKey, &using_default_name)) {
      // If the preference hasn't been set, and the name is default, assume
      // that the user hasn't done this on purpose.
      using_default_name = IsDefaultProfileName(name);
      info->SetBoolean(kIsUsingDefaultNameKey, using_default_name);
    }

    // For profiles that don't have the "using default avatar" state set yet,
    // assume it's the same as the "using default name" state.
    if (!info->HasKey(kIsUsingDefaultAvatarKey)) {
      info->SetBoolean(kIsUsingDefaultAvatarKey, using_default_name);
    }
  }

  // If needed, start downloading the high-res avatars and migrate any legacy
  // profile names.
  if (!disable_avatar_download_for_testing_)
    MigrateLegacyProfileNamesAndDownloadAvatars();
}

ProfileInfoCache::~ProfileInfoCache() {
}

void ProfileInfoCache::AddProfileToCache(
    const base::FilePath& profile_path,
    const base::string16& name,
    const std::string& gaia_id,
    const base::string16& user_name,
    size_t icon_index,
    const std::string& supervised_user_id) {
  std::string key = CacheKeyFromProfilePath(profile_path);
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();

  std::unique_ptr<base::DictionaryValue> info(new base::DictionaryValue);
  info->SetString(kNameKey, name);
  info->SetString(kGAIAIdKey, gaia_id);
  info->SetString(kUserNameKey, user_name);
  info->SetString(kAvatarIconKey,
      profiles::GetDefaultAvatarIconUrl(icon_index));
  // Default value for whether background apps are running is false.
  info->SetBoolean(kBackgroundAppsKey, false);
  info->SetString(kSupervisedUserId, supervised_user_id);
  info->SetBoolean(kIsOmittedFromProfileListKey, !supervised_user_id.empty());
  info->SetBoolean(kProfileIsEphemeral, false);
  info->SetBoolean(kIsUsingDefaultNameKey, IsDefaultProfileName(name));
  // Assume newly created profiles use a default avatar.
  info->SetBoolean(kIsUsingDefaultAvatarKey, true);
  cache->SetWithoutPathExpansion(key, info.release());

  sorted_keys_.insert(FindPositionForProfile(key, name), key);
  profile_attributes_entries_[user_data_dir_.AppendASCII(key).value()] =
      std::unique_ptr<ProfileAttributesEntry>();

  if (!disable_avatar_download_for_testing_)
    DownloadHighResAvatarIfNeeded(icon_index, profile_path);

  for (auto& observer : observer_list_)
    observer.OnProfileAdded(profile_path);
}

void ProfileInfoCache::AddObserver(ProfileInfoCacheObserver* obs) {
  observer_list_.AddObserver(obs);
}

void ProfileInfoCache::RemoveObserver(ProfileInfoCacheObserver* obs) {
  observer_list_.RemoveObserver(obs);
}

void ProfileInfoCache::DeleteProfileFromCache(
    const base::FilePath& profile_path) {
  size_t profile_index = GetIndexOfProfileWithPath(profile_path);
  if (profile_index == std::string::npos) {
    NOTREACHED();
    return;
  }
  base::string16 name = GetNameOfProfileAtIndex(profile_index);

  for (auto& observer : observer_list_)
    observer.OnProfileWillBeRemoved(profile_path);

  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  std::string key = CacheKeyFromProfilePath(profile_path);
  cache->Remove(key, NULL);
  sorted_keys_.erase(std::find(sorted_keys_.begin(), sorted_keys_.end(), key));
  profile_attributes_entries_.erase(profile_path.value());

  for (auto& observer : observer_list_)
    observer.OnProfileWasRemoved(profile_path, name);
}

size_t ProfileInfoCache::GetNumberOfProfiles() const {
  return sorted_keys_.size();
}

size_t ProfileInfoCache::GetIndexOfProfileWithPath(
    const base::FilePath& profile_path) const {
  if (profile_path.DirName() != user_data_dir_)
    return std::string::npos;
  std::string search_key = CacheKeyFromProfilePath(profile_path);
  for (size_t i = 0; i < sorted_keys_.size(); ++i) {
    if (sorted_keys_[i] == search_key)
      return i;
  }
  return std::string::npos;
}

base::string16 ProfileInfoCache::GetNameOfProfileAtIndex(size_t index) const {
  base::string16 name;
  // Unless the user has customized the profile name, we should use the
  // profile's Gaia given name, if it's available.
  if (ProfileIsUsingDefaultNameAtIndex(index)) {
    base::string16 given_name = GetGAIAGivenNameOfProfileAtIndex(index);
    name = given_name.empty() ? GetGAIANameOfProfileAtIndex(index) : given_name;
  }
  if (name.empty())
    GetInfoForProfileAtIndex(index)->GetString(kNameKey, &name);
  return name;
}

base::string16 ProfileInfoCache::GetShortcutNameOfProfileAtIndex(size_t index)
    const {
  base::string16 shortcut_name;
  GetInfoForProfileAtIndex(index)->GetString(
      kShortcutNameKey, &shortcut_name);
  return shortcut_name;
}

base::FilePath ProfileInfoCache::GetPathOfProfileAtIndex(size_t index) const {
  return user_data_dir_.AppendASCII(sorted_keys_[index]);
}

base::Time ProfileInfoCache::GetProfileActiveTimeAtIndex(size_t index) const {
  double dt;
  if (GetInfoForProfileAtIndex(index)->GetDouble(kActiveTimeKey, &dt)) {
    return base::Time::FromDoubleT(dt);
  } else {
    return base::Time();
  }
}

base::string16 ProfileInfoCache::GetUserNameOfProfileAtIndex(
    size_t index) const {
  base::string16 user_name;
  GetInfoForProfileAtIndex(index)->GetString(kUserNameKey, &user_name);
  return user_name;
}

const gfx::Image& ProfileInfoCache::GetAvatarIconOfProfileAtIndex(
    size_t index) const {
  if (IsUsingGAIAPictureOfProfileAtIndex(index)) {
    const gfx::Image* image = GetGAIAPictureOfProfileAtIndex(index);
    if (image)
      return *image;
  }

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  // Use the high resolution version of the avatar if it exists. Mobile and
  // ChromeOS don't need the high resolution version so no need to fetch it.
  const gfx::Image* image = GetHighResAvatarOfProfileAtIndex(index);
  if (image)
    return *image;
#endif

  int resource_id = profiles::GetDefaultAvatarIconResourceIDAtIndex(
      GetAvatarIconIndexOfProfileAtIndex(index));
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
}

std::string ProfileInfoCache::GetLocalAuthCredentialsOfProfileAtIndex(
    size_t index) const {
  std::string credentials;
  GetInfoForProfileAtIndex(index)->GetString(kAuthCredentialsKey, &credentials);
  return credentials;
}

std::string ProfileInfoCache::GetPasswordChangeDetectionTokenAtIndex(
    size_t index) const {
  std::string token;
  GetInfoForProfileAtIndex(index)->GetString(kPasswordTokenKey, &token);
  return token;
}

bool ProfileInfoCache::GetBackgroundStatusOfProfileAtIndex(
    size_t index) const {
  bool background_app_status;
  if (!GetInfoForProfileAtIndex(index)->GetBoolean(kBackgroundAppsKey,
                                                   &background_app_status)) {
    return false;
  }
  return background_app_status;
}

base::string16 ProfileInfoCache::GetGAIANameOfProfileAtIndex(
    size_t index) const {
  base::string16 name;
  GetInfoForProfileAtIndex(index)->GetString(kGAIANameKey, &name);
  return name;
}

base::string16 ProfileInfoCache::GetGAIAGivenNameOfProfileAtIndex(
    size_t index) const {
  base::string16 name;
  GetInfoForProfileAtIndex(index)->GetString(kGAIAGivenNameKey, &name);
  return name;
}

std::string ProfileInfoCache::GetGAIAIdOfProfileAtIndex(
    size_t index) const {
  std::string gaia_id;
  GetInfoForProfileAtIndex(index)->GetString(kGAIAIdKey, &gaia_id);
  return gaia_id;
}

const gfx::Image* ProfileInfoCache::GetGAIAPictureOfProfileAtIndex(
    size_t index) const {
  base::FilePath path = GetPathOfProfileAtIndex(index);
  std::string key = CacheKeyFromProfilePath(path);

  std::string file_name;
  GetInfoForProfileAtIndex(index)->GetString(
      kGAIAPictureFileNameKey, &file_name);

  // If the picture is not on disk then return NULL.
  if (file_name.empty())
    return NULL;

  base::FilePath image_path = path.AppendASCII(file_name);
  return LoadAvatarPictureFromPath(path, key, image_path);
}

bool ProfileInfoCache::IsUsingGAIAPictureOfProfileAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kUseGAIAPictureKey, &value);
  if (!value) {
    // Prefer the GAIA avatar over a non-customized avatar.
    value = ProfileIsUsingDefaultAvatarAtIndex(index) &&
        GetGAIAPictureOfProfileAtIndex(index);
  }
  return value;
}

bool ProfileInfoCache::ProfileIsSupervisedAtIndex(size_t index) const {
  return !GetSupervisedUserIdOfProfileAtIndex(index).empty();
}

bool ProfileInfoCache::ProfileIsChildAtIndex(size_t index) const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  return GetSupervisedUserIdOfProfileAtIndex(index) ==
      supervised_users::kChildAccountSUID;
#else
  return false;
#endif
}

bool ProfileInfoCache::ProfileIsLegacySupervisedAtIndex(size_t index) const {
  return ProfileIsSupervisedAtIndex(index) && !ProfileIsChildAtIndex(index);
}

bool ProfileInfoCache::IsOmittedProfileAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kIsOmittedFromProfileListKey,
                                              &value);
  return value;
}

bool ProfileInfoCache::ProfileIsSigninRequiredAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kSigninRequiredKey, &value);
  return value;
}

std::string ProfileInfoCache::GetSupervisedUserIdOfProfileAtIndex(
    size_t index) const {
  std::string supervised_user_id;
  GetInfoForProfileAtIndex(index)->GetString(kSupervisedUserId,
                                             &supervised_user_id);
  return supervised_user_id;
}

bool ProfileInfoCache::ProfileIsEphemeralAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kProfileIsEphemeral, &value);
  return value;
}

bool ProfileInfoCache::ProfileIsUsingDefaultNameAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kIsUsingDefaultNameKey, &value);
  return value;
}

bool ProfileInfoCache::ProfileIsAuthenticatedAtIndex(size_t index) const {
  // The profile is authenticated if the gaia_id of the info is not empty.
  // If it is empty, also check if the user name is not empty.  This latter
  // check is needed in case the profile has not been loaded yet and the
  // gaia_id property has not yet been written.
  return !GetGAIAIdOfProfileAtIndex(index).empty() ||
      !GetUserNameOfProfileAtIndex(index).empty();
}

bool ProfileInfoCache::ProfileIsUsingDefaultAvatarAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kIsUsingDefaultAvatarKey, &value);
  return value;
}

bool ProfileInfoCache::ProfileIsAuthErrorAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kIsAuthErrorKey, &value);
  return value;
}

size_t ProfileInfoCache::GetAvatarIconIndexOfProfileAtIndex(size_t index)
    const {
  std::string icon_url;
  GetInfoForProfileAtIndex(index)->GetString(kAvatarIconKey, &icon_url);
  size_t icon_index = 0;
  if (!profiles::IsDefaultAvatarIconUrl(icon_url, &icon_index))
    DLOG(WARNING) << "Unknown avatar icon: " << icon_url;

  return icon_index;
}

bool ProfileInfoCache::HasStatsBrowsingHistoryOfProfileAtIndex(size_t index)
    const {
  int value = 0;
  return GetInfoForProfileAtIndex(index)->GetInteger(kStatsBrowsingHistoryKey,
                                                     &value);
}

int ProfileInfoCache::GetStatsBrowsingHistoryOfProfileAtIndex(size_t index)
    const {
  int value = 0;
  GetInfoForProfileAtIndex(index)->GetInteger(kStatsBrowsingHistoryKey, &value);
  return value;
}

bool ProfileInfoCache::HasStatsPasswordsOfProfileAtIndex(size_t index) const {
  int value = 0;
  return GetInfoForProfileAtIndex(index)->GetInteger(kStatsPasswordsKey,
                                                     &value);
}

int ProfileInfoCache::GetStatsPasswordsOfProfileAtIndex(size_t index) const {
  int value = 0;
  GetInfoForProfileAtIndex(index)->GetInteger(kStatsPasswordsKey, &value);
  return value;
}

bool ProfileInfoCache::HasStatsBookmarksOfProfileAtIndex(size_t index) const {
  int value = 0;
  return GetInfoForProfileAtIndex(index)->GetInteger(kStatsBookmarksKey,
                                                     &value);
}

int ProfileInfoCache::GetStatsBookmarksOfProfileAtIndex(size_t index) const {
  int value = 0;
  GetInfoForProfileAtIndex(index)->GetInteger(kStatsBookmarksKey, &value);
  return value;
}

bool ProfileInfoCache::HasStatsSettingsOfProfileAtIndex(size_t index) const {
  int value = 0;
  return GetInfoForProfileAtIndex(index)->GetInteger(kStatsSettingsKey, &value);
}

int ProfileInfoCache::GetStatsSettingsOfProfileAtIndex(size_t index) const {
  int value = 0;
  GetInfoForProfileAtIndex(index)->GetInteger(kStatsSettingsKey, &value);
  return value;
}

void ProfileInfoCache::SetProfileActiveTimeAtIndex(size_t index) {
  if (base::Time::Now() - GetProfileActiveTimeAtIndex(index) <
      base::TimeDelta::FromHours(1)) {
    return;
  }

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetDouble(kActiveTimeKey, base::Time::Now().ToDoubleT());
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetNameOfProfileAtIndex(size_t index,
                                               const base::string16& name) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  base::string16 current_name;
  info->GetString(kNameKey, &current_name);
  if (name == current_name)
    return;

  base::string16 old_display_name = GetNameOfProfileAtIndex(index);
  info->SetString(kNameKey, name);

  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::string16 new_display_name = GetNameOfProfileAtIndex(index);
  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  UpdateSortForProfileIndex(index);

  if (old_display_name != new_display_name) {
    for (auto& observer : observer_list_)
      observer.OnProfileNameChanged(profile_path, old_display_name);
  }
}

void ProfileInfoCache::SetShortcutNameOfProfileAtIndex(
    size_t index,
    const base::string16& shortcut_name) {
  if (shortcut_name == GetShortcutNameOfProfileAtIndex(index))
    return;
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kShortcutNameKey, shortcut_name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetAuthInfoOfProfileAtIndex(
    size_t index,
    const std::string& gaia_id,
    const base::string16& user_name) {
  // If both gaia_id and username are unchanged, abort early.
  if (gaia_id == GetGAIAIdOfProfileAtIndex(index) &&
      user_name == GetUserNameOfProfileAtIndex(index)) {
    return;
  }

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());

  info->SetString(kGAIAIdKey, gaia_id);
  info->SetString(kUserNameKey, user_name);

  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  for (auto& observer : observer_list_)
    observer.OnProfileAuthInfoChanged(profile_path);
}

void ProfileInfoCache::SetAvatarIconOfProfileAtIndex(size_t index,
                                                     size_t icon_index) {
  if (!profiles::IsDefaultAvatarIconIndex(icon_index)) {
    DLOG(WARNING) << "Unknown avatar icon index: " << icon_index;
    // switch to generic avatar
    icon_index = 0;
  }
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kAvatarIconKey,
      profiles::GetDefaultAvatarIconUrl(icon_index));
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);

  if (!disable_avatar_download_for_testing_)
    DownloadHighResAvatarIfNeeded(icon_index, profile_path);

  for (auto& observer : observer_list_)
    observer.OnProfileAvatarChanged(profile_path);
}

void ProfileInfoCache::SetIsOmittedProfileAtIndex(size_t index,
                                                  bool is_omitted) {
  if (IsOmittedProfileAtIndex(index) == is_omitted)
    return;
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsOmittedFromProfileListKey, is_omitted);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  for (auto& observer : observer_list_)
    observer.OnProfileIsOmittedChanged(profile_path);
}

void ProfileInfoCache::SetSupervisedUserIdOfProfileAtIndex(
    size_t index,
    const std::string& id) {
  if (GetSupervisedUserIdOfProfileAtIndex(index) == id)
    return;
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kSupervisedUserId, id);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  for (auto& observer : observer_list_)
    observer.OnProfileSupervisedUserIdChanged(profile_path);
}

void ProfileInfoCache::SetLocalAuthCredentialsOfProfileAtIndex(
    size_t index,
    const std::string& credentials) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kAuthCredentialsKey, credentials);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetPasswordChangeDetectionTokenAtIndex(
    size_t index,
    const std::string& token) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kPasswordTokenKey, token);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetBackgroundStatusOfProfileAtIndex(
    size_t index,
    bool running_background_apps) {
  if (GetBackgroundStatusOfProfileAtIndex(index) == running_background_apps)
    return;
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kBackgroundAppsKey, running_background_apps);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetGAIANameOfProfileAtIndex(size_t index,
                                                   const base::string16& name) {
  if (name == GetGAIANameOfProfileAtIndex(index))
    return;

  base::string16 old_display_name = GetNameOfProfileAtIndex(index);
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kGAIANameKey, name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
  base::string16 new_display_name = GetNameOfProfileAtIndex(index);
  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  UpdateSortForProfileIndex(index);

  if (old_display_name != new_display_name) {
    for (auto& observer : observer_list_)
      observer.OnProfileNameChanged(profile_path, old_display_name);
  }
}

void ProfileInfoCache::SetGAIAGivenNameOfProfileAtIndex(
    size_t index,
    const base::string16& name) {
  if (name == GetGAIAGivenNameOfProfileAtIndex(index))
    return;

  base::string16 old_display_name = GetNameOfProfileAtIndex(index);
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kGAIAGivenNameKey, name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
  base::string16 new_display_name = GetNameOfProfileAtIndex(index);
  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  UpdateSortForProfileIndex(index);

  if (old_display_name != new_display_name) {
    for (auto& observer : observer_list_)
      observer.OnProfileNameChanged(profile_path, old_display_name);
  }
}

void ProfileInfoCache::SetGAIAPictureOfProfileAtIndex(size_t index,
                                                      const gfx::Image* image) {
  base::FilePath path = GetPathOfProfileAtIndex(index);
  std::string key = CacheKeyFromProfilePath(path);

  // Delete the old bitmap from cache.
  cached_avatar_images_.erase(key);

  std::string old_file_name;
  GetInfoForProfileAtIndex(index)->GetString(
      kGAIAPictureFileNameKey, &old_file_name);
  std::string new_file_name;

  if (!image) {
    // Delete the old bitmap from disk.
    if (!old_file_name.empty()) {
      base::FilePath image_path = path.AppendASCII(old_file_name);
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                              base::Bind(&DeleteBitmap, image_path));
    }
  } else {
    // Save the new bitmap to disk.
    new_file_name =
        old_file_name.empty() ? profiles::kGAIAPictureFileName : old_file_name;
    base::FilePath image_path = path.AppendASCII(new_file_name);
    SaveAvatarImageAtPath(
        GetPathOfProfileAtIndex(index), image, key, image_path);
  }

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kGAIAPictureFileNameKey, new_file_name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  for (auto& observer : observer_list_)
    observer.OnProfileAvatarChanged(path);
}

void ProfileInfoCache::SetIsUsingGAIAPictureOfProfileAtIndex(size_t index,
                                                             bool value) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kUseGAIAPictureKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  for (auto& observer : observer_list_)
    observer.OnProfileAvatarChanged(profile_path);
}

void ProfileInfoCache::SetProfileSigninRequiredAtIndex(size_t index,
                                                       bool value) {
  if (value == ProfileIsSigninRequiredAtIndex(index))
    return;

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kSigninRequiredKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  for (auto& observer : observer_list_)
    observer.OnProfileSigninRequiredChanged(profile_path);
}

void ProfileInfoCache::SetProfileIsEphemeralAtIndex(size_t index, bool value) {
  if (value == ProfileIsEphemeralAtIndex(index))
    return;

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kProfileIsEphemeral, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetProfileIsUsingDefaultNameAtIndex(
    size_t index, bool value) {
  if (value == ProfileIsUsingDefaultNameAtIndex(index))
    return;

  base::string16 old_display_name = GetNameOfProfileAtIndex(index);

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsUsingDefaultNameKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::string16 new_display_name = GetNameOfProfileAtIndex(index);
  const base::FilePath profile_path = GetPathOfProfileAtIndex(index);

  if (old_display_name != new_display_name) {
    for (auto& observer : observer_list_)
      observer.OnProfileNameChanged(profile_path, old_display_name);
  }
}

void ProfileInfoCache::SetProfileIsUsingDefaultAvatarAtIndex(
    size_t index, bool value) {
  if (value == ProfileIsUsingDefaultAvatarAtIndex(index))
    return;

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsUsingDefaultAvatarKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetProfileIsAuthErrorAtIndex(size_t index, bool value) {
  if (value == ProfileIsAuthErrorAtIndex(index))
    return;

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsAuthErrorKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetStatsBrowsingHistoryOfProfileAtIndex(size_t index,
                                                               int value) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetInteger(kStatsBrowsingHistoryKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetStatsPasswordsOfProfileAtIndex(size_t index,
                                                         int value) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetInteger(kStatsPasswordsKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetStatsBookmarksOfProfileAtIndex(size_t index,
                                                         int value) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetInteger(kStatsBookmarksKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetStatsSettingsOfProfileAtIndex(size_t index,
                                                        int value) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetInteger(kStatsSettingsKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

const base::FilePath& ProfileInfoCache::GetUserDataDir() const {
  return user_data_dir_;
}

// static
void ProfileInfoCache::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProfileInfoCache);
}

void ProfileInfoCache::DownloadHighResAvatarIfNeeded(
    size_t icon_index,
    const base::FilePath& profile_path) {
  // Downloading is only supported on desktop.
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  return;
#endif
  DCHECK(!disable_avatar_download_for_testing_);

  // If this is the placeholder avatar, it is already included in the
  // resources, so it doesn't need to be downloaded (and it will never be
  // requested from disk by GetHighResAvatarOfProfileAtIndex).
  if (icon_index == profiles::GetPlaceholderAvatarIndex())
    return;

  const base::FilePath& file_path =
      profiles::GetPathOfHighResAvatarAtIndex(icon_index);
  base::Closure callback =
      base::Bind(&ProfileInfoCache::DownloadHighResAvatar,
                 AsWeakPtr(),
                 icon_index,
                 profile_path);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&RunCallbackIfFileMissing, file_path, callback));
}

void ProfileInfoCache::SaveAvatarImageAtPath(
    const base::FilePath& profile_path,
    const gfx::Image* image,
    const std::string& key,
    const base::FilePath& image_path) {
  cached_avatar_images_[key].reset(new gfx::Image(*image));

  std::unique_ptr<ImageData> data(new ImageData);
  scoped_refptr<base::RefCountedMemory> png_data = image->As1xPNGBytes();
  data->assign(png_data->front(), png_data->front() + png_data->size());

  // Remove the file from the list of downloads in progress. Note that this list
  // only contains the high resolution avatars, and not the Gaia profile images.
  auto downloader_iter = avatar_images_downloads_in_progress_.find(key);
  if (downloader_iter != avatar_images_downloads_in_progress_.end()) {
    // We mustn't delete the avatar downloader right here, since we're being
    // called by it.
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE,
                              downloader_iter->second.release());
    avatar_images_downloads_in_progress_.erase(downloader_iter);
  }

  if (data->empty()) {
    LOG(ERROR) << "Failed to PNG encode the image.";
  } else {
    base::Closure callback = base::Bind(&ProfileInfoCache::OnAvatarPictureSaved,
        AsWeakPtr(), key, profile_path);
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&SaveBitmap, base::Passed(&data), image_path, callback));
  }
}

const base::DictionaryValue* ProfileInfoCache::GetInfoForProfileAtIndex(
    size_t index) const {
  DCHECK_LT(index, GetNumberOfProfiles());
  const base::DictionaryValue* cache =
      prefs_->GetDictionary(prefs::kProfileInfoCache);
  const base::DictionaryValue* info = NULL;
  cache->GetDictionaryWithoutPathExpansion(sorted_keys_[index], &info);
  return info;
}

void ProfileInfoCache::SetInfoForProfileAtIndex(size_t index,
                                                base::DictionaryValue* info) {
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  cache->SetWithoutPathExpansion(sorted_keys_[index], info);
}

std::string ProfileInfoCache::CacheKeyFromProfilePath(
    const base::FilePath& profile_path) const {
  DCHECK(user_data_dir_ == profile_path.DirName());
  base::FilePath base_name = profile_path.BaseName();
  return base_name.MaybeAsASCII();
}

std::vector<std::string>::iterator ProfileInfoCache::FindPositionForProfile(
    const std::string& search_key,
    const base::string16& search_name) {
  base::string16 search_name_l = base::i18n::ToLower(search_name);
  for (size_t i = 0; i < GetNumberOfProfiles(); ++i) {
    base::string16 name_l = base::i18n::ToLower(GetNameOfProfileAtIndex(i));
    int name_compare = search_name_l.compare(name_l);
    if (name_compare < 0)
      return sorted_keys_.begin() + i;
    if (name_compare == 0) {
      int key_compare = search_key.compare(sorted_keys_[i]);
      if (key_compare < 0)
        return sorted_keys_.begin() + i;
    }
  }
  return sorted_keys_.end();
}

void ProfileInfoCache::UpdateSortForProfileIndex(size_t index) {
  base::string16 name = GetNameOfProfileAtIndex(index);

  // Remove and reinsert key in |sorted_keys_| to alphasort.
  std::string key = CacheKeyFromProfilePath(GetPathOfProfileAtIndex(index));
  std::vector<std::string>::iterator key_it =
      std::find(sorted_keys_.begin(), sorted_keys_.end(), key);
  DCHECK(key_it != sorted_keys_.end());
  sorted_keys_.erase(key_it);
  sorted_keys_.insert(FindPositionForProfile(key, name), key);
}

const gfx::Image* ProfileInfoCache::GetHighResAvatarOfProfileAtIndex(
    size_t index) const {
  const size_t avatar_index = GetAvatarIconIndexOfProfileAtIndex(index);

  // If this is the placeholder avatar, it is already included in the
  // resources, so it doesn't need to be downloaded.
  if (avatar_index == profiles::GetPlaceholderAvatarIndex()) {
    return &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }

  const std::string file_name =
      profiles::GetDefaultAvatarIconFileNameAtIndex(avatar_index);
  const base::FilePath image_path =
      profiles::GetPathOfHighResAvatarAtIndex(avatar_index);
  return LoadAvatarPictureFromPath(GetPathOfProfileAtIndex(index), file_name,
                                   image_path);
}

void ProfileInfoCache::DownloadHighResAvatar(
    size_t icon_index,
    const base::FilePath& profile_path) {
  // Downloading is only supported on desktop.
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  return;
#endif
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461175
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461175 ProfileInfoCache::DownloadHighResAvatar::GetFileName"));
  const char* file_name =
      profiles::GetDefaultAvatarIconFileNameAtIndex(icon_index);
  DCHECK(file_name);
  // If the file is already being downloaded, don't start another download.
  if (avatar_images_downloads_in_progress_.count(file_name))
    return;

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461175
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461175 ProfileInfoCache::DownloadHighResAvatar::MakeDownloader"));
  // Start the download for this file. The cache takes ownership of the
  // avatar downloader, which will be deleted when the download completes, or
  // if that never happens, when the ProfileInfoCache is destroyed.
  std::unique_ptr<ProfileAvatarDownloader>& current_downloader =
      avatar_images_downloads_in_progress_[file_name];
  current_downloader.reset(
      new ProfileAvatarDownloader(
          icon_index,
          base::Bind(&ProfileInfoCache::SaveAvatarImageAtPath,
                     base::Unretained(this),
                     profile_path)));

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461175
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461175 ProfileInfoCache::DownloadHighResAvatar::StartDownload"));
  current_downloader->Start();
}

const gfx::Image* ProfileInfoCache::LoadAvatarPictureFromPath(
    const base::FilePath& profile_path,
    const std::string& key,
    const base::FilePath& image_path) const {
  // If the picture is already loaded then use it.
  if (cached_avatar_images_.count(key)) {
    if (cached_avatar_images_[key]->IsEmpty())
      return NULL;
    return cached_avatar_images_[key].get();
  }

  // Don't download the image if downloading is disabled for tests.
  if (disable_avatar_download_for_testing_)
    return NULL;

  // If the picture is already being loaded then don't try loading it again.
  if (cached_avatar_images_loading_[key])
    return NULL;
  cached_avatar_images_loading_[key] = true;

  gfx::Image** image = new gfx::Image*;
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&ReadBitmap, image_path, image),
      base::Bind(&ProfileInfoCache::OnAvatarPictureLoaded,
          const_cast<ProfileInfoCache*>(this)->AsWeakPtr(),
          profile_path, key, image));
  return NULL;
}

void ProfileInfoCache::OnAvatarPictureLoaded(const base::FilePath& profile_path,
                                             const std::string& key,
                                             gfx::Image** image) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461175
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461175 ProfileInfoCache::OnAvatarPictureLoaded::Start"));

  cached_avatar_images_loading_[key] = false;

  if (*image) {
    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461175
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile2(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "461175 ProfileInfoCache::OnAvatarPictureLoaded::SetImage"));
    cached_avatar_images_[key].reset(*image);
  } else {
    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461175
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile3(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "461175 ProfileInfoCache::OnAvatarPictureLoaded::MakeEmptyImage"));
    // Place an empty image in the cache to avoid reloading it again.
    cached_avatar_images_[key].reset(new gfx::Image());
  }
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461175
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461175 ProfileInfoCache::OnAvatarPictureLoaded::DeleteImage"));
  delete image;

  for (auto& observer : observer_list_)
    observer.OnProfileHighResAvatarLoaded(profile_path);
}

void ProfileInfoCache::OnAvatarPictureSaved(
      const std::string& file_name,
      const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnProfileHighResAvatarLoaded(profile_path);
}

void ProfileInfoCache::MigrateLegacyProfileNamesAndDownloadAvatars() {
  // Only do this on desktop platforms.
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Migrate any legacy profile names ("First user", "Default Profile") to
  // new style default names ("Person 1"). The problem here is that every
  // time you rename a profile, the ProfileInfoCache sorts itself, so
  // whatever you were iterating through is no longer valid. We need to
  // save a list of the profile paths (which thankfully do not change) that
  // need to be renamed. We also can't pre-compute the new names, as they
  // depend on the names of all the other profiles in the info cache, so they
  // need to be re-computed after each rename.
  std::vector<base::FilePath> profiles_to_rename;

  const base::string16 default_profile_name = base::i18n::ToLower(
      l10n_util::GetStringUTF16(IDS_DEFAULT_PROFILE_NAME));
  const base::string16 default_legacy_profile_name = base::i18n::ToLower(
      l10n_util::GetStringUTF16(IDS_LEGACY_DEFAULT_PROFILE_NAME));

  for (size_t i = 0; i < GetNumberOfProfiles(); i++) {
    DownloadHighResAvatarIfNeeded(GetAvatarIconIndexOfProfileAtIndex(i),
                                  GetPathOfProfileAtIndex(i));

    base::string16 name = base::i18n::ToLower(GetNameOfProfileAtIndex(i));
    if (name == default_profile_name || name == default_legacy_profile_name)
      profiles_to_rename.push_back(GetPathOfProfileAtIndex(i));
  }

  // Rename the necessary profiles.
  std::vector<base::FilePath>::const_iterator it;
  for (it = profiles_to_rename.begin(); it != profiles_to_rename.end(); ++it) {
    size_t profile_index = GetIndexOfProfileWithPath(*it);
    SetProfileIsUsingDefaultNameAtIndex(profile_index, true);
    // This will assign a new "Person %d" type name and re-sort the cache.
    SetNameOfProfileAtIndex(profile_index, ChooseNameForNewProfile(
        GetAvatarIconIndexOfProfileAtIndex(profile_index)));
  }
#endif
}

void ProfileInfoCache::AddProfile(
    const base::FilePath& profile_path,
    const base::string16& name,
    const std::string& gaia_id,
    const base::string16& user_name,
    size_t icon_index,
    const std::string& supervised_user_id) {
  AddProfileToCache(
      profile_path, name, gaia_id, user_name, icon_index, supervised_user_id);
}

void ProfileInfoCache::RemoveProfile(const base::FilePath& profile_path) {
  DeleteProfileFromCache(profile_path);
}

std::vector<ProfileAttributesEntry*>
ProfileInfoCache::GetAllProfilesAttributes() {
  std::vector<ProfileAttributesEntry*> ret;
  for (size_t i = 0; i < GetNumberOfProfiles(); ++i) {
    ProfileAttributesEntry* entry;
    if (GetProfileAttributesWithPath(GetPathOfProfileAtIndex(i), &entry)) {
      ret.push_back(entry);
    }
  }
  return ret;
}

bool ProfileInfoCache::GetProfileAttributesWithPath(
    const base::FilePath& path, ProfileAttributesEntry** entry) {
  const auto entry_iter = profile_attributes_entries_.find(path.value());
  if (entry_iter == profile_attributes_entries_.end())
    return false;

  std::unique_ptr<ProfileAttributesEntry>& current_entry = entry_iter->second;
  if (!current_entry) {
    // The profile info is in the cache but its entry isn't created yet, insert
    // it in the map.
    current_entry.reset(new ProfileAttributesEntry());
    current_entry->Initialize(this, path);
  }

  *entry = current_entry.get();
  return true;
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_avatar_downloader.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

using content::BrowserThread;

namespace {

const char kNameKey[] = "name";
const char kShortcutNameKey[] = "shortcut_name";
const char kGAIANameKey[] = "gaia_name";
const char kGAIAGivenNameKey[] = "gaia_given_name";
const char kUserNameKey[] = "user_name";
const char kIsUsingDefaultNameKey[] = "is_using_default_name";
const char kIsUsingDefaultAvatarKey[] = "is_using_default_avatar";
const char kAvatarIconKey[] = "avatar_icon";
const char kAuthCredentialsKey[] = "local_auth_credentials";
const char kUseGAIAPictureKey[] = "use_gaia_picture";
const char kBackgroundAppsKey[] = "background_apps";
const char kGAIAPictureFileNameKey[] = "gaia_picture_file_name";
const char kIsOmittedFromProfileListKey[] = "is_omitted_from_profile_list";
const char kSigninRequiredKey[] = "signin_required";
const char kSupervisedUserId[] = "managed_user_id";
const char kProfileIsEphemeral[] = "is_ephemeral";
const char kActiveTimeKey[] = "active_time";

// First eight are generic icons, which use IDS_NUMBERED_PROFILE_NAME.
const int kDefaultNames[] = {
  IDS_DEFAULT_AVATAR_NAME_8,
  IDS_DEFAULT_AVATAR_NAME_9,
  IDS_DEFAULT_AVATAR_NAME_10,
  IDS_DEFAULT_AVATAR_NAME_11,
  IDS_DEFAULT_AVATAR_NAME_12,
  IDS_DEFAULT_AVATAR_NAME_13,
  IDS_DEFAULT_AVATAR_NAME_14,
  IDS_DEFAULT_AVATAR_NAME_15,
  IDS_DEFAULT_AVATAR_NAME_16,
  IDS_DEFAULT_AVATAR_NAME_17,
  IDS_DEFAULT_AVATAR_NAME_18,
  IDS_DEFAULT_AVATAR_NAME_19,
  IDS_DEFAULT_AVATAR_NAME_20,
  IDS_DEFAULT_AVATAR_NAME_21,
  IDS_DEFAULT_AVATAR_NAME_22,
  IDS_DEFAULT_AVATAR_NAME_23,
  IDS_DEFAULT_AVATAR_NAME_24,
  IDS_DEFAULT_AVATAR_NAME_25,
  IDS_DEFAULT_AVATAR_NAME_26
};

typedef std::vector<unsigned char> ImageData;

// Writes |data| to disk and takes ownership of the pointer. On successful
// completion, it runs |callback|.
void SaveBitmap(scoped_ptr<ImageData> data,
                const base::FilePath& image_path,
                const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
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

void DeleteBitmap(const base::FilePath& image_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::DeleteFile(image_path, false);
}

}  // namespace

ProfileInfoCache::ProfileInfoCache(PrefService* prefs,
                                   const base::FilePath& user_data_dir)
    : prefs_(prefs),
      user_data_dir_(user_data_dir) {
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
  if (switches::IsNewAvatarMenu())
    MigrateLegacyProfileNamesAndDownloadAvatars();
}

ProfileInfoCache::~ProfileInfoCache() {
  STLDeleteContainerPairSecondPointers(
      cached_avatar_images_.begin(), cached_avatar_images_.end());
  STLDeleteContainerPairSecondPointers(
      avatar_images_downloads_in_progress_.begin(),
      avatar_images_downloads_in_progress_.end());
}

void ProfileInfoCache::AddProfileToCache(
    const base::FilePath& profile_path,
    const base::string16& name,
    const base::string16& username,
    size_t icon_index,
    const std::string& supervised_user_id) {
  std::string key = CacheKeyFromProfilePath(profile_path);
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();

  scoped_ptr<base::DictionaryValue> info(new base::DictionaryValue);
  info->SetString(kNameKey, name);
  info->SetString(kUserNameKey, username);
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

  if (switches::IsNewAvatarMenu())
    DownloadHighResAvatar(icon_index, profile_path);

  FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                    observer_list_,
                    OnProfileAdded(profile_path));

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
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

  FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                    observer_list_,
                    OnProfileWillBeRemoved(profile_path));

  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  std::string key = CacheKeyFromProfilePath(profile_path);
  cache->Remove(key, NULL);
  sorted_keys_.erase(std::find(sorted_keys_.begin(), sorted_keys_.end(), key));

  FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                    observer_list_,
                    OnProfileWasRemoved(profile_path, name));

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
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

  // Use the high resolution version of the avatar if it exists.
  if (switches::IsNewAvatarMenu()) {
    const gfx::Image* image = GetHighResAvatarOfProfileAtIndex(index);
    if (image)
      return *image;
  }

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
  return LoadAvatarPictureFromPath(key, image_path);
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

bool ProfileInfoCache::ProfileIsUsingDefaultAvatarAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kIsUsingDefaultAvatarKey, &value);
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

void ProfileInfoCache::SetProfileActiveTimeAtIndex(size_t index) {
  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetDouble(kActiveTimeKey, base::Time::Now().ToDoubleT());
  // This takes ownership of |info|.
  SetInfoQuietlyForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetNameOfProfileAtIndex(size_t index,
                                               const base::string16& name) {
  scoped_ptr<base::DictionaryValue> info(
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
    FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                      observer_list_,
                      OnProfileNameChanged(profile_path, old_display_name));
  }
}

void ProfileInfoCache::SetShortcutNameOfProfileAtIndex(
    size_t index,
    const base::string16& shortcut_name) {
  if (shortcut_name == GetShortcutNameOfProfileAtIndex(index))
    return;
  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kShortcutNameKey, shortcut_name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetUserNameOfProfileAtIndex(
    size_t index,
    const base::string16& user_name) {
  if (user_name == GetUserNameOfProfileAtIndex(index))
    return;

  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kUserNameKey, user_name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetAvatarIconOfProfileAtIndex(size_t index,
                                                     size_t icon_index) {
  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kAvatarIconKey,
      profiles::GetDefaultAvatarIconUrl(icon_index));
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);

  // If needed, start downloading the high-res avatar.
  if (switches::IsNewAvatarMenu())
    DownloadHighResAvatar(icon_index, profile_path);

  FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                    observer_list_,
                    OnProfileAvatarChanged(profile_path));
}

void ProfileInfoCache::SetIsOmittedProfileAtIndex(size_t index,
                                                  bool is_omitted) {
  if (IsOmittedProfileAtIndex(index) == is_omitted)
    return;
  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsOmittedFromProfileListKey, is_omitted);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetSupervisedUserIdOfProfileAtIndex(
    size_t index,
    const std::string& id) {
  if (GetSupervisedUserIdOfProfileAtIndex(index) == id)
    return;
  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kSupervisedUserId, id);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                    observer_list_,
                    OnProfileSupervisedUserIdChanged(profile_path));
}

void ProfileInfoCache::SetLocalAuthCredentialsOfProfileAtIndex(
    size_t index,
    const std::string& credentials) {
  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kAuthCredentialsKey, credentials);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetBackgroundStatusOfProfileAtIndex(
    size_t index,
    bool running_background_apps) {
  if (GetBackgroundStatusOfProfileAtIndex(index) == running_background_apps)
    return;
  scoped_ptr<base::DictionaryValue> info(
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
  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kGAIANameKey, name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
  base::string16 new_display_name = GetNameOfProfileAtIndex(index);
  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  UpdateSortForProfileIndex(index);

  if (old_display_name != new_display_name) {
    FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                      observer_list_,
                      OnProfileNameChanged(profile_path, old_display_name));
  }
}

void ProfileInfoCache::SetGAIAGivenNameOfProfileAtIndex(
    size_t index,
    const base::string16& name) {
  if (name == GetGAIAGivenNameOfProfileAtIndex(index))
    return;

  base::string16 old_display_name = GetNameOfProfileAtIndex(index);
  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kGAIAGivenNameKey, name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
  base::string16 new_display_name = GetNameOfProfileAtIndex(index);
  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  UpdateSortForProfileIndex(index);

  if (old_display_name != new_display_name) {
    FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                      observer_list_,
                      OnProfileNameChanged(profile_path, old_display_name));
  }
}

void ProfileInfoCache::SetGAIAPictureOfProfileAtIndex(size_t index,
                                                      const gfx::Image* image) {
  base::FilePath path = GetPathOfProfileAtIndex(index);
  std::string key = CacheKeyFromProfilePath(path);

  // Delete the old bitmap from cache.
  std::map<std::string, gfx::Image*>::iterator it =
      cached_avatar_images_.find(key);
  if (it != cached_avatar_images_.end()) {
    delete it->second;
    cached_avatar_images_.erase(it);
  }

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
        image, key, image_path, GetPathOfProfileAtIndex(index));
  }

  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kGAIAPictureFileNameKey, new_file_name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                    observer_list_,
                    OnProfileAvatarChanged(path));
}

void ProfileInfoCache::SetIsUsingGAIAPictureOfProfileAtIndex(size_t index,
                                                             bool value) {
  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kUseGAIAPictureKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  // Retrieve some info to update observers who care about avatar changes.
  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                    observer_list_,
                    OnProfileAvatarChanged(profile_path));
}

void ProfileInfoCache::SetProfileSigninRequiredAtIndex(size_t index,
                                                       bool value) {
  if (value == ProfileIsSigninRequiredAtIndex(index))
    return;

  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kSigninRequiredKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                    observer_list_,
                    OnProfileSigninRequiredChanged(profile_path));
}

void ProfileInfoCache::SetProfileIsEphemeralAtIndex(size_t index, bool value) {
  if (value == ProfileIsEphemeralAtIndex(index))
    return;

  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kProfileIsEphemeral, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetProfileIsUsingDefaultNameAtIndex(
    size_t index, bool value) {
  if (value == ProfileIsUsingDefaultNameAtIndex(index))
    return;

  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsUsingDefaultNameKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetProfileIsUsingDefaultAvatarAtIndex(
    size_t index, bool value) {
  if (value == ProfileIsUsingDefaultAvatarAtIndex(index))
    return;

  scoped_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsUsingDefaultAvatarKey, value);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

bool ProfileInfoCache::IsDefaultProfileName(const base::string16& name) const {
  // Check if it's a "First user" old-style name.
  if (name == l10n_util::GetStringUTF16(IDS_DEFAULT_PROFILE_NAME) ||
      name == l10n_util::GetStringUTF16(IDS_LEGACY_DEFAULT_PROFILE_NAME))
    return true;

  // Check if it's one of the old-style profile names.
  for (size_t i = 0; i < arraysize(kDefaultNames); ++i) {
    if (name == l10n_util::GetStringUTF16(kDefaultNames[i]))
      return true;
  }

  // Check whether it's one of the "Person %d" style names.
  std::string default_name_format = l10n_util::GetStringFUTF8(
      IDS_NEW_NUMBERED_PROFILE_NAME, base::ASCIIToUTF16("%d"));

  int generic_profile_number;  // Unused. Just a placeholder for sscanf.
  int assignments = sscanf(base::UTF16ToUTF8(name).c_str(),
                           default_name_format.c_str(),
                           &generic_profile_number);
  // Unless it matched the format, this is a custom name.
  return assignments == 1;
}

base::string16 ProfileInfoCache::ChooseNameForNewProfile(
    size_t icon_index) const {
  base::string16 name;
  for (int name_index = 1; ; ++name_index) {
    if (switches::IsNewAvatarMenu()) {
      name = l10n_util::GetStringFUTF16Int(IDS_NEW_NUMBERED_PROFILE_NAME,
                                           name_index);
    } else if (icon_index < profiles::GetGenericAvatarIconCount()) {
      name = l10n_util::GetStringFUTF16Int(IDS_NUMBERED_PROFILE_NAME,
                                           name_index);
    } else {
      name = l10n_util::GetStringUTF16(
          kDefaultNames[icon_index - profiles::GetGenericAvatarIconCount()]);
      if (name_index > 1)
        name.append(base::UTF8ToUTF16(base::IntToString(name_index)));
    }

    // Loop through previously named profiles to ensure we're not duplicating.
    bool name_found = false;
    for (size_t i = 0; i < GetNumberOfProfiles(); ++i) {
      if (GetNameOfProfileAtIndex(i) == name) {
        name_found = true;
        break;
      }
    }
    if (!name_found)
      return name;
  }
}

size_t ProfileInfoCache::ChooseAvatarIconIndexForNewProfile() const {
  size_t icon_index = 0;
  // Try to find a unique, non-generic icon.
  if (ChooseAvatarIconIndexForNewProfile(false, true, &icon_index))
    return icon_index;
  // Try to find any unique icon.
  if (ChooseAvatarIconIndexForNewProfile(true, true, &icon_index))
    return icon_index;
  // Settle for any random icon, even if it's not unique.
  if (ChooseAvatarIconIndexForNewProfile(true, false, &icon_index))
    return icon_index;

  NOTREACHED();
  return 0;
}

const base::FilePath& ProfileInfoCache::GetUserDataDir() const {
  return user_data_dir_;
}

// static
std::vector<base::string16> ProfileInfoCache::GetProfileNames() {
  std::vector<base::string16> names;
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* cache = local_state->GetDictionary(
      prefs::kProfileInfoCache);
  base::string16 name;
  for (base::DictionaryValue::Iterator it(*cache); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* info = NULL;
    it.value().GetAsDictionary(&info);
    info->GetString(kNameKey, &name);
    names.push_back(name);
  }
  return names;
}

// static
void ProfileInfoCache::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProfileInfoCache);
}

void ProfileInfoCache::DownloadHighResAvatar(
    size_t icon_index,
    const base::FilePath& profile_path) {
  // Downloading is only supported on desktop.
#if defined(OS_ANDROID) || defined(OS_IOS) || defined(OS_CHROMEOS)
  return;
#endif

  // TODO(noms): We should check whether the file already exists on disk
  // before trying to re-download it. For now, since this is behind a flag and
  // the resources are still changing, re-download it every time the profile
  // avatar changes, to make sure we have the latest copy.
  std::string file_name = profiles::GetDefaultAvatarIconFileNameAtIndex(
      icon_index);
  // If the file is already being downloaded, don't start another download.
  if (avatar_images_downloads_in_progress_[file_name])
    return;

  // Start the download for this file. The cache takes ownership of the
  // |avatar_downloader|, which will be deleted when the download completes, or
  // if that never happens, when the ProfileInfoCache is destroyed.
  ProfileAvatarDownloader* avatar_downloader = new ProfileAvatarDownloader(
      icon_index,
      profile_path,
      this);
  avatar_images_downloads_in_progress_[file_name] = avatar_downloader;
  avatar_downloader->Start();
}

void ProfileInfoCache::SaveAvatarImageAtPath(
    const gfx::Image* image,
    const std::string& key,
    const base::FilePath& image_path,
    const base::FilePath& profile_path) {
  cached_avatar_images_[key] = new gfx::Image(*image);

  scoped_ptr<ImageData> data(new ImageData);
  scoped_refptr<base::RefCountedMemory> png_data = image->As1xPNGBytes();
  data->assign(png_data->front(), png_data->front() + png_data->size());

  if (!data->size()) {
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

void ProfileInfoCache::SetInfoQuietlyForProfileAtIndex(
    size_t index, base::DictionaryValue* info) {
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  cache->SetWithoutPathExpansion(sorted_keys_[index], info);
}

// TODO(noms): Switch to newer notification system.
void ProfileInfoCache::SetInfoForProfileAtIndex(size_t index,
                                                base::DictionaryValue* info) {
  SetInfoQuietlyForProfileAtIndex(index, info);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
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

bool ProfileInfoCache::IconIndexIsUnique(size_t icon_index) const {
  for (size_t i = 0; i < GetNumberOfProfiles(); ++i) {
    if (GetAvatarIconIndexOfProfileAtIndex(i) == icon_index)
      return false;
  }
  return true;
}

bool ProfileInfoCache::ChooseAvatarIconIndexForNewProfile(
    bool allow_generic_icon,
    bool must_be_unique,
    size_t* out_icon_index) const {
  // Always allow all icons for new profiles if using the
  // --new-avatar-menu flag.
  if (switches::IsNewAvatarMenu())
    allow_generic_icon = true;
  size_t start = allow_generic_icon ? 0 : profiles::GetGenericAvatarIconCount();
  size_t end = profiles::GetDefaultAvatarIconCount();
  size_t count = end - start;

  int rand = base::RandInt(0, count);
  for (size_t i = 0; i < count; ++i) {
    size_t icon_index = start + (rand + i) %  count;
    if (!must_be_unique || IconIndexIsUnique(icon_index)) {
      *out_icon_index = icon_index;
      return true;
    }
  }

  return false;
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

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

const gfx::Image* ProfileInfoCache::GetHighResAvatarOfProfileAtIndex(
    size_t index) const {
  int avatar_index = GetAvatarIconIndexOfProfileAtIndex(index);
  std::string key = profiles::GetDefaultAvatarIconFileNameAtIndex(avatar_index);

  if (!strcmp(key.c_str(), profiles::GetNoHighResAvatarFileName()))
      return NULL;

  base::FilePath image_path =
      profiles::GetPathOfHighResAvatarAtIndex(avatar_index);
  return LoadAvatarPictureFromPath(key, image_path);
}

const gfx::Image* ProfileInfoCache::LoadAvatarPictureFromPath(
    const std::string& key,
    const base::FilePath& image_path) const {
  // If the picture is already loaded then use it.
  if (cached_avatar_images_.count(key)) {
    if (cached_avatar_images_[key]->IsEmpty())
      return NULL;
    return cached_avatar_images_[key];
  }

  // If the picture is already being loaded then don't try loading it again.
  if (cached_avatar_images_loading_[key])
    return NULL;
  cached_avatar_images_loading_[key] = true;

  gfx::Image** image = new gfx::Image*;
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&ReadBitmap, image_path, image),
      base::Bind(&ProfileInfoCache::OnAvatarPictureLoaded,
          const_cast<ProfileInfoCache*>(this)->AsWeakPtr(), key, image));
  return NULL;
}

void ProfileInfoCache::OnAvatarPictureLoaded(const std::string& key,
                                             gfx::Image** image) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  cached_avatar_images_loading_[key] = false;
  delete cached_avatar_images_[key];

  if (*image) {
    cached_avatar_images_[key] = *image;
  } else {
    // Place an empty image in the cache to avoid reloading it again.
    cached_avatar_images_[key] = new gfx::Image();
  }
  delete image;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void ProfileInfoCache::OnAvatarPictureSaved(
      const std::string& file_name,
      const base::FilePath& profile_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHE_PICTURE_SAVED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  FOR_EACH_OBSERVER(ProfileInfoCacheObserver,
                    observer_list_,
                    OnProfileAvatarChanged(profile_path));

  // Remove the file from the list of downloads in progress. Note that this list
  // only contains the high resolution avatars, and not the Gaia profile images.
  if (!avatar_images_downloads_in_progress_[file_name])
    return;

  delete avatar_images_downloads_in_progress_[file_name];
  avatar_images_downloads_in_progress_[file_name] = NULL;
}

void ProfileInfoCache::MigrateLegacyProfileNamesAndDownloadAvatars() {
  DCHECK(switches::IsNewAvatarMenu());

  // Only do this on desktop platforms.
#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
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
    // If needed, start downloading the high-res avatar for this profile.
    DownloadHighResAvatar(GetAvatarIconIndexOfProfileAtIndex(i),
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

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kNameKey[] = "name";
const char kUserNameKey[] = "user_name";
const char kAvatarIconKey[] = "avatar_icon";
const char kDefaultUrlPrefix[] = "chrome://theme/IDR_PROFILE_AVATAR_";

const int kDefaultAvatarIconResources[] = {
  IDR_PROFILE_AVATAR_0,
  IDR_PROFILE_AVATAR_1,
  IDR_PROFILE_AVATAR_2,
  IDR_PROFILE_AVATAR_3,
  IDR_PROFILE_AVATAR_4,
  IDR_PROFILE_AVATAR_5,
  IDR_PROFILE_AVATAR_6,
  IDR_PROFILE_AVATAR_7,
  IDR_PROFILE_AVATAR_8,
  IDR_PROFILE_AVATAR_9,
  IDR_PROFILE_AVATAR_10,
  IDR_PROFILE_AVATAR_11,
  IDR_PROFILE_AVATAR_12,
  IDR_PROFILE_AVATAR_13,
  IDR_PROFILE_AVATAR_14,
  IDR_PROFILE_AVATAR_15,
  IDR_PROFILE_AVATAR_16,
  IDR_PROFILE_AVATAR_17,
  IDR_PROFILE_AVATAR_18,
  IDR_PROFILE_AVATAR_19,
  IDR_PROFILE_AVATAR_20,
  IDR_PROFILE_AVATAR_21,
  IDR_PROFILE_AVATAR_22,
  IDR_PROFILE_AVATAR_23,
  IDR_PROFILE_AVATAR_24,
  IDR_PROFILE_AVATAR_25,
};

const size_t kDefaultAvatarIconsCount = arraysize(kDefaultAvatarIconResources);

// Returns true if the resource ID belongs to a generic avatar icon.
bool IsAvatarIconGeneric(int icon_id) {
  return icon_id == IDR_PROFILE_AVATAR_0 ||
         icon_id == IDR_PROFILE_AVATAR_1 ||
         icon_id == IDR_PROFILE_AVATAR_2 ||
         icon_id == IDR_PROFILE_AVATAR_3 ||
         icon_id == IDR_PROFILE_AVATAR_4 ||
         icon_id == IDR_PROFILE_AVATAR_5 ||
         icon_id == IDR_PROFILE_AVATAR_6 ||
         icon_id == IDR_PROFILE_AVATAR_7;
}

} // namespace

ProfileInfoCache::ProfileInfoCache(PrefService* prefs,
                                   const FilePath& user_data_dir)
    : prefs_(prefs),
      user_data_dir_(user_data_dir) {
  // Populate the cache
  const DictionaryValue* cache =
      prefs_->GetDictionary(prefs::kProfileInfoCache);
  for (DictionaryValue::key_iterator it = cache->begin_keys();
       it != cache->end_keys(); ++it) {
    std::string key = *it;
    DictionaryValue* info = NULL;
    cache->GetDictionary(key, &info);
    string16 name;
    info->GetString(kNameKey, &name);
    sorted_keys_.insert(FindPositionForProfile(key, name), key);
  }
}

ProfileInfoCache::~ProfileInfoCache() {
}

void ProfileInfoCache::AddProfileToCache(const FilePath& profile_path,
                                         const string16& name,
                                         size_t icon_index) {
  std::string key = CacheKeyFromProfilePath(profile_path);
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  DictionaryValue* cache = update.Get();

  scoped_ptr<DictionaryValue> info(new DictionaryValue);
  info->SetString(kNameKey, name);
  info->SetString(kAvatarIconKey, GetDefaultAvatarIconUrl(icon_index));
  cache->Set(key, info.release());

  sorted_keys_.insert(FindPositionForProfile(key, name), key);

  NotificationService::current()->Notify(
    chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
    NotificationService::AllSources(),
    NotificationService::NoDetails());
}

void ProfileInfoCache::DeleteProfileFromCache(const FilePath& profile_path) {
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  DictionaryValue* cache = update.Get();

  std::string key = CacheKeyFromProfilePath(profile_path);
  cache->Remove(key, NULL);
  sorted_keys_.erase(std::find(sorted_keys_.begin(), sorted_keys_.end(), key));

  NotificationService::current()->Notify(
    chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
    NotificationService::AllSources(),
    NotificationService::NoDetails());
}

size_t ProfileInfoCache::GetNumberOfProfiles() const {
  return sorted_keys_.size();
}

size_t ProfileInfoCache::GetIndexOfProfileWithPath(
    const FilePath& profile_path) const {
  if (profile_path.DirName() != user_data_dir_)
    return std::string::npos;
  std::string search_key = CacheKeyFromProfilePath(profile_path);
  for (size_t i = 0; i < sorted_keys_.size(); ++i) {
    if (sorted_keys_[i] == search_key)
      return i;
  }
  return std::string::npos;
}

string16 ProfileInfoCache::GetNameOfProfileAtIndex(size_t index) const {
  string16 name;
  GetInfoForProfileAtIndex(index)->GetString(kNameKey, &name);
  return name;
}

FilePath ProfileInfoCache::GetPathOfProfileAtIndex(size_t index) const {
  FilePath::StringType base_name;
#if defined(OS_POSIX)
  base_name = sorted_keys_[index];
#elif defined(OS_WIN)
  base_name = ASCIIToWide(sorted_keys_[index]);
#endif
  return user_data_dir_.Append(base_name);
}

string16 ProfileInfoCache::GetUserNameOfProfileAtIndex(size_t index) const {
  string16 user_name;
  GetInfoForProfileAtIndex(index)->GetString(kUserNameKey, &user_name);
  return user_name;
}

const gfx::Image& ProfileInfoCache::GetAvatarIconOfProfileAtIndex(
    size_t index) const {
  int resource_id = GetDefaultAvatarIconResourceIDAtIndex(
      GetAvatarIconIndexOfProfileAtIndex(index));
  return ResourceBundle::GetSharedInstance().GetImageNamed(resource_id);
}

size_t ProfileInfoCache::GetAvatarIconIndexOfProfileAtIndex(size_t index)
    const {
  std::string icon_url;
  GetInfoForProfileAtIndex(index)->GetString(kAvatarIconKey, &icon_url);
  size_t icon_index = 0;
  if (IsDefaultAvatarIconUrl(icon_url, &icon_index))
    return icon_index;

  DLOG(WARNING) << "Unknown avatar icon: " << icon_url;
  return GetDefaultAvatarIconResourceIDAtIndex(0);
}

void ProfileInfoCache::SetNameOfProfileAtIndex(size_t index,
                                               const string16& name) {
  scoped_ptr<DictionaryValue> info(GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kNameKey, name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetUserNameOfProfileAtIndex(size_t index,
                                                   const string16& user_name) {
  scoped_ptr<DictionaryValue> info(GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kUserNameKey, user_name);
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

void ProfileInfoCache::SetAvatarIconOfProfileAtIndex(size_t index,
                                                     size_t icon_index) {
  scoped_ptr<DictionaryValue> info(GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kAvatarIconKey, GetDefaultAvatarIconUrl(icon_index));
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

string16 ProfileInfoCache::ChooseNameForNewProfile() {
  for (int name_index = 1; ; ++name_index) {
    string16 name = l10n_util::GetStringFUTF16Int(
        IDS_NUMBERED_PROFILE_NAME, name_index);
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

int ProfileInfoCache::ChooseAvatarIconIndexForNewProfile() {
  // Start with a random icon to introduce variety.
  size_t rand_start_index = base::RandInt(0, GetDefaultAvatarIconCount() - 1);
  for (size_t icon_index = 0; icon_index < GetDefaultAvatarIconCount();
       ++icon_index) {
    size_t rand_icon_index =
        (icon_index + rand_start_index) % GetDefaultAvatarIconCount();
    if (IsAvatarIconGeneric(GetDefaultAvatarIconResourceIDAtIndex(
        rand_icon_index)))
      continue;

    bool icon_found = false;
    for (size_t i = 0; i < GetNumberOfProfiles(); ++i) {
      if (GetAvatarIconIndexOfProfileAtIndex(i) == rand_icon_index) {
        icon_found = true;
        break;
      }
    }
    if (!icon_found)
      return rand_icon_index;
  }

  // If there's no unique icon then just use the random one.
  return rand_start_index;
}

const FilePath& ProfileInfoCache::GetUserDataDir() const {
  return user_data_dir_;
}

// static
size_t ProfileInfoCache::GetDefaultAvatarIconCount() {
  return kDefaultAvatarIconsCount;
}

// static
int ProfileInfoCache::GetDefaultAvatarIconResourceIDAtIndex(size_t index) {
  DCHECK_LT(index, GetDefaultAvatarIconCount());
  return kDefaultAvatarIconResources[index];
}

// static
std::string ProfileInfoCache::GetDefaultAvatarIconUrl(size_t index) {
  DCHECK_LT(index, kDefaultAvatarIconsCount);
  return StringPrintf("%s%" PRIuS, kDefaultUrlPrefix, index);
}

// static
bool ProfileInfoCache::IsDefaultAvatarIconUrl(const std::string& url,
                                              size_t* icon_index) {
  DCHECK(icon_index);
  if (url.find(kDefaultUrlPrefix) != 0)
    return false;

  int int_value = -1;
  if (base::StringToInt(url.begin() + strlen(kDefaultUrlPrefix),
                        url.end(),
                        &int_value)) {
    if (int_value < 0 ||
        int_value >= static_cast<int>(kDefaultAvatarIconsCount))
      return false;
    *icon_index = int_value;
    return true;
  }

  return false;
}

const DictionaryValue* ProfileInfoCache::GetInfoForProfileAtIndex(
    size_t index) const {
  DCHECK_LT(index, GetNumberOfProfiles());
  const DictionaryValue* cache =
      prefs_->GetDictionary(prefs::kProfileInfoCache);
  DictionaryValue* info = NULL;
  cache->GetDictionary(sorted_keys_[index], &info);
  return info;
}

void ProfileInfoCache::SetInfoForProfileAtIndex(size_t index,
                                                DictionaryValue* info) {
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  DictionaryValue* cache = update.Get();
  cache->Set(sorted_keys_[index], info);

  NotificationService::current()->Notify(
    chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
    NotificationService::AllSources(),
    NotificationService::NoDetails());
}

std::string ProfileInfoCache::CacheKeyFromProfilePath(
    const FilePath& profile_path) const {
  DCHECK(user_data_dir_ == profile_path.DirName());
  FilePath base_name = profile_path.BaseName();
  return base_name.MaybeAsASCII();
}

std::vector<std::string>::iterator ProfileInfoCache::FindPositionForProfile(
    std::string search_key,
    const string16& search_name) {
  for (size_t i = 0; i < GetNumberOfProfiles(); ++i) {
    int name_compare = search_name.compare(GetNameOfProfileAtIndex(i));
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

void ProfileInfoCache::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kProfileInfoCache);
}

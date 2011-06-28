// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kNameKey[] = "name";
const char kAvatarIconKey[] = "avatar_icon";
const char kDefaultUrlPrefix[] = "chrome://theme/IDR_PROFILE_AVATAR_";

const int kDefaultAvatarIconResources[] = {
  IDR_PROFILE_AVATAR_0,
  IDR_PROFILE_AVATAR_1,
  IDR_PROFILE_AVATAR_2,
  IDR_PROFILE_AVATAR_3,
};

const int kDefaultAvatarIconsCount = arraysize(kDefaultAvatarIconResources);

// Checks if the given URL points to one of the default avatar icons. if it is,
// returns true and its index through |icon_index|. If not, returns false.
bool IsDefaultAvatarIconUrl(const std::string& url, size_t* icon_index) {
  DCHECK(icon_index);
  if (url.find(kDefaultUrlPrefix) != 0)
    return false;

  int int_value = -1;
  if (base::StringToInt(url.begin() + strlen(kDefaultUrlPrefix),
                        url.end(),
                        &int_value)) {
    if (int_value < 0 || int_value >= kDefaultAvatarIconsCount)
      return false;
    *icon_index = int_value;
    return true;
  }

  return false;
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
}

void ProfileInfoCache::DeleteProfileFromCache(const FilePath& profile_path) {
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  DictionaryValue* cache = update.Get();

  std::string key = CacheKeyFromProfilePath(profile_path);
  cache->Remove(key, NULL);
  sorted_keys_.erase(std::find(sorted_keys_.begin(), sorted_keys_.end(), key));
}

size_t ProfileInfoCache::GetNumberOfProfiles() const {
  return sorted_keys_.size();
}

size_t ProfileInfoCache::GetIndexOfProfileWithPath(
    const FilePath& profile_path) const {
  if (profile_path.DirName() != user_data_dir_)
    return std::string::npos;
  std::string search_key = profile_path.BaseName().MaybeAsASCII();
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

void ProfileInfoCache::SetAvatarIconOfProfileAtIndex(size_t index,
                                                     size_t icon_index) {
  scoped_ptr<DictionaryValue> info(GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kAvatarIconKey, GetDefaultAvatarIconUrl(icon_index));
  // This takes ownership of |info|.
  SetInfoForProfileAtIndex(index, info.release());
}

size_t ProfileInfoCache::GetDefaultAvatarIconCount() {
  return kDefaultAvatarIconsCount;
}

int ProfileInfoCache::GetDefaultAvatarIconResourceIDAtIndex(size_t index) {
  DCHECK_LT(index, GetDefaultAvatarIconCount());
  return kDefaultAvatarIconResources[index];
}

std::string ProfileInfoCache::GetDefaultAvatarIconUrl(size_t index) {
  int icon_index = index;
  DCHECK_LT(icon_index, kDefaultAvatarIconsCount);
  return StringPrintf("%s%d", kDefaultUrlPrefix, icon_index);
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

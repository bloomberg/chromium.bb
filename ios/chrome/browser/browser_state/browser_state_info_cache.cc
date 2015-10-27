// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache_observer.h"
#include "ios/chrome/browser/pref_names.h"

namespace {
const char kGAIAIdKey[] = "gaia_id";
const char kIsAuthErrorKey[] = "is_auth_error";
const char kNameKey[] = "name";
const char kUserNameKey[] = "user_name";
}

BrowserStateInfoCache::BrowserStateInfoCache(
    PrefService* prefs,
    const base::FilePath& user_data_dir)
    : prefs_(prefs), user_data_dir_(user_data_dir) {
  // Populate the cache
  DictionaryPrefUpdate update(prefs_, ios::prefs::kBrowserStateInfoCache);
  base::DictionaryValue* cache = update.Get();
  for (base::DictionaryValue::Iterator it(*cache); !it.IsAtEnd();
       it.Advance()) {
    base::DictionaryValue* info = nullptr;
    cache->GetDictionaryWithoutPathExpansion(it.key(), &info);
    base::string16 name;
    info->GetString(kNameKey, &name);
    sorted_keys_.insert(FindPositionForBrowserState(it.key(), name), it.key());
  }
}

BrowserStateInfoCache::~BrowserStateInfoCache() {}

void BrowserStateInfoCache::AddBrowserState(
    const base::FilePath& browser_state_path,
    const base::string16& name,
    const std::string& gaia_id,
    const base::string16& user_name) {
  std::string key = CacheKeyFromBrowserStatePath(browser_state_path);
  DictionaryPrefUpdate update(prefs_, ios::prefs::kBrowserStateInfoCache);
  base::DictionaryValue* cache = update.Get();

  scoped_ptr<base::DictionaryValue> info(new base::DictionaryValue);
  info->SetString(kNameKey, name);
  info->SetString(kGAIAIdKey, gaia_id);
  info->SetString(kUserNameKey, user_name);
  cache->SetWithoutPathExpansion(key, info.release());

  sorted_keys_.insert(FindPositionForBrowserState(key, name), key);

  FOR_EACH_OBSERVER(BrowserStateInfoCacheObserver, observer_list_,
                    OnBrowserStateAdded(browser_state_path));
}

void BrowserStateInfoCache::AddObserver(
    BrowserStateInfoCacheObserver* observer) {
  observer_list_.AddObserver(observer);
}

void BrowserStateInfoCache::RemoveObserver(
    BrowserStateInfoCacheObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowserStateInfoCache::RemoveBrowserState(
    const base::FilePath& browser_state_path) {
  size_t browser_state_index =
      GetIndexOfBrowserStateWithPath(browser_state_path);
  if (browser_state_index == std::string::npos) {
    NOTREACHED();
    return;
  }
  base::string16 name = GetNameOfBrowserStateAtIndex(browser_state_index);

  DictionaryPrefUpdate update(prefs_, ios::prefs::kBrowserStateInfoCache);
  base::DictionaryValue* cache = update.Get();
  std::string key = CacheKeyFromBrowserStatePath(browser_state_path);
  cache->Remove(key, nullptr);
  sorted_keys_.erase(std::find(sorted_keys_.begin(), sorted_keys_.end(), key));

  FOR_EACH_OBSERVER(BrowserStateInfoCacheObserver, observer_list_,
                    OnBrowserStateWasRemoved(browser_state_path, name));
}

size_t BrowserStateInfoCache::GetNumberOfBrowserStates() const {
  return sorted_keys_.size();
}

size_t BrowserStateInfoCache::GetIndexOfBrowserStateWithPath(
    const base::FilePath& browser_state_path) const {
  if (browser_state_path.DirName() != user_data_dir_)
    return std::string::npos;
  std::string search_key = CacheKeyFromBrowserStatePath(browser_state_path);
  for (size_t i = 0; i < sorted_keys_.size(); ++i) {
    if (sorted_keys_[i] == search_key)
      return i;
  }
  return std::string::npos;
}

base::string16 BrowserStateInfoCache::GetNameOfBrowserStateAtIndex(
    size_t index) const {
  base::string16 name;
  GetInfoForBrowserStateAtIndex(index)->GetString(kNameKey, &name);
  return name;
}

base::string16 BrowserStateInfoCache::GetUserNameOfBrowserStateAtIndex(
    size_t index) const {
  base::string16 user_name;
  GetInfoForBrowserStateAtIndex(index)->GetString(kUserNameKey, &user_name);
  return user_name;
}

base::FilePath BrowserStateInfoCache::GetPathOfBrowserStateAtIndex(
    size_t index) const {
  return user_data_dir_.AppendASCII(sorted_keys_[index]);
}

std::string BrowserStateInfoCache::GetGAIAIdOfBrowserStateAtIndex(
    size_t index) const {
  std::string gaia_id;
  GetInfoForBrowserStateAtIndex(index)->GetString(kGAIAIdKey, &gaia_id);
  return gaia_id;
}

bool BrowserStateInfoCache::BrowserStateIsAuthenticatedAtIndex(
    size_t index) const {
  // The browser state is authenticated if the gaia_id of the info is not empty.
  // If it is empty, also check if the user name is not empty.  This latter
  // check is needed in case the browser state has not been loaded yet and the
  // gaia_id property has not yet been written.
  return !GetGAIAIdOfBrowserStateAtIndex(index).empty() ||
         !GetUserNameOfBrowserStateAtIndex(index).empty();
}

bool BrowserStateInfoCache::BrowserStateIsAuthErrorAtIndex(size_t index) const {
  bool value = false;
  GetInfoForBrowserStateAtIndex(index)->GetBoolean(kIsAuthErrorKey, &value);
  return value;
}

void BrowserStateInfoCache::SetAuthInfoOfBrowserStateAtIndex(
    size_t index,
    const std::string& gaia_id,
    const base::string16& user_name) {
  // If both gaia_id and username are unchanged, abort early.
  if (gaia_id == GetGAIAIdOfBrowserStateAtIndex(index) &&
      user_name == GetUserNameOfBrowserStateAtIndex(index)) {
    return;
  }

  scoped_ptr<base::DictionaryValue> info(
      GetInfoForBrowserStateAtIndex(index)->DeepCopy());

  info->SetString(kGAIAIdKey, gaia_id);
  info->SetString(kUserNameKey, user_name);

  // This takes ownership of |info|.
  SetInfoForBrowserStateAtIndex(index, info.release());
}

void BrowserStateInfoCache::SetBrowserStateIsAuthErrorAtIndex(size_t index,
                                                              bool value) {
  if (value == BrowserStateIsAuthErrorAtIndex(index))
    return;

  scoped_ptr<base::DictionaryValue> info(
      GetInfoForBrowserStateAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsAuthErrorKey, value);
  // This takes ownership of |info|.
  SetInfoForBrowserStateAtIndex(index, info.release());
}

const base::FilePath& BrowserStateInfoCache::GetUserDataDir() const {
  return user_data_dir_;
}

// static
void BrowserStateInfoCache::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(ios::prefs::kBrowserStateInfoCache);
}

const base::DictionaryValue*
BrowserStateInfoCache::GetInfoForBrowserStateAtIndex(size_t index) const {
  DCHECK_LT(index, GetNumberOfBrowserStates());
  const base::DictionaryValue* cache =
      prefs_->GetDictionary(ios::prefs::kBrowserStateInfoCache);
  const base::DictionaryValue* info = nullptr;
  cache->GetDictionaryWithoutPathExpansion(sorted_keys_[index], &info);
  return info;
}

void BrowserStateInfoCache::SetInfoForBrowserStateAtIndex(
    size_t index,
    base::DictionaryValue* info) {
  DictionaryPrefUpdate update(prefs_, ios::prefs::kBrowserStateInfoCache);
  base::DictionaryValue* cache = update.Get();
  cache->SetWithoutPathExpansion(sorted_keys_[index], info);
}

std::string BrowserStateInfoCache::CacheKeyFromBrowserStatePath(
    const base::FilePath& browser_state_path) const {
  DCHECK(user_data_dir_ == browser_state_path.DirName());
  base::FilePath base_name = browser_state_path.BaseName();
  return base_name.MaybeAsASCII();
}

std::vector<std::string>::iterator
BrowserStateInfoCache::FindPositionForBrowserState(
    const std::string& search_key,
    const base::string16& search_name) {
  base::string16 search_name_l = base::i18n::ToLower(search_name);
  for (size_t i = 0; i < GetNumberOfBrowserStates(); ++i) {
    base::string16 name_l =
        base::i18n::ToLower(GetNameOfBrowserStateAtIndex(i));
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

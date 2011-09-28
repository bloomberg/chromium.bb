// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_storage_cache.h"

ExtensionSettingsStorageCache::ExtensionSettingsStorageCache(
    ExtensionSettingsStorage* delegate) : delegate_(delegate) {}

ExtensionSettingsStorageCache::~ExtensionSettingsStorageCache() {}

ExtensionSettingsStorage::Result ExtensionSettingsStorageCache::Get(
    const std::string& key) {
  Value *value;
  if (GetFromCache(key, &value)) {
    DictionaryValue* settings = new DictionaryValue();
    settings->SetWithoutPathExpansion(key, value);
    return Result(settings);
  }

  Result result = delegate_->Get(key);
  if (result.HasError()) {
    return result;
  }

  cache_.MergeDictionary(result.GetSettings());
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageCache::Get(
    const std::vector<std::string>& keys) {
  scoped_ptr<DictionaryValue> from_cache(new DictionaryValue());
  std::vector<std::string> missing_keys;

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    Value *value;
    if (GetFromCache(*it, &value)) {
      from_cache->SetWithoutPathExpansion(*it, value);
    } else {
      missing_keys.push_back(*it);
    }
  }

  if (missing_keys.empty()) {
    return Result(from_cache.release());
  }

  Result result = delegate_->Get(keys);
  if (result.HasError()) {
    return result;
  }

  cache_.MergeDictionary(result.GetSettings());
  result.GetSettings()->MergeDictionary(from_cache.get());
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageCache::Get() {
  Result result = delegate_->Get();
  if (result.HasError()) {
    return result;
  }

  cache_.MergeDictionary(result.GetSettings());
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageCache::Set(
    const std::string& key, const Value& value) {
  Result result = delegate_->Set(key, value);
  if (result.HasError()) {
    return result;
  }

  cache_.MergeDictionary(result.GetSettings());
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageCache::Set(
    const DictionaryValue& settings) {
  Result result = delegate_->Set(settings);
  if (result.HasError()) {
    return result;
  }

  cache_.MergeDictionary(result.GetSettings());
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageCache::Remove(
    const std::string& key) {
  Result result = delegate_->Remove(key);
  if (result.HasError()) {
    return result;
  }

  cache_.RemoveWithoutPathExpansion(key, NULL);
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageCache::Remove(
    const std::vector<std::string>& keys) {
  Result result = delegate_->Remove(keys);
  if (result.HasError()) {
    return result;
  }

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    cache_.RemoveWithoutPathExpansion(*it, NULL);
  }
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageCache::Clear() {
  Result result = delegate_->Clear();
  if (result.HasError()) {
    return result;
  }

  cache_.Clear();
  return result;
}

bool ExtensionSettingsStorageCache::GetFromCache(
    const std::string& key, Value** value) {
  Value* cached_value;
  if (!cache_.GetWithoutPathExpansion(key, &cached_value)) {
    return false;
  }

  *value = cached_value->DeepCopy();
  return true;
}

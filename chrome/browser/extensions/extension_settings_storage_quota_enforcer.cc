// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_storage_quota_enforcer.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"

namespace {

const char* kExceededQuotaErrorMessage = "Quota exceeded";

// Allocates a setting in a record of total and per-setting usage.
void Allocate(
    const std::string& key,
    const Value& value,
    size_t* used_total,
    std::map<std::string, size_t>* used_per_setting) {
  // Calculate the setting size based on its JSON serialization size.
  // TODO(kalman): Does this work with different encodings?
  // TODO(kalman): This is duplicating work that the leveldb delegate
  // implementation is about to do, and it would be nice to avoid this.
  std::string value_as_json;
  base::JSONWriter::Write(&value, false, &value_as_json);
  size_t new_size = key.size() + value_as_json.size();
  size_t existing_size = (*used_per_setting)[key];

  *used_total += (new_size - existing_size);
  (*used_per_setting)[key] = new_size;
}

// Frees the allocation of a setting in a record of total and per-setting usage.
void Free(
    size_t* used_total,
    std::map<std::string, size_t>* used_per_setting,
    const std::string& key) {
  *used_total -= (*used_per_setting)[key];
  used_per_setting->erase(key);
}

}  // namespace

ExtensionSettingsStorageQuotaEnforcer::ExtensionSettingsStorageQuotaEnforcer(
    size_t quota_bytes,
    size_t quota_bytes_per_setting,
    size_t max_keys,
    ExtensionSettingsStorage* delegate)
    : quota_bytes_(quota_bytes),
      quota_bytes_per_setting_(quota_bytes_per_setting),
      max_keys_(max_keys),
      delegate_(delegate),
      used_total_(0) {
  Result maybe_initial_settings = delegate->Get();
  if (maybe_initial_settings.HasError()) {
    LOG(WARNING) << "Failed to get initial settings for quota: " <<
        maybe_initial_settings.GetError();
    return;
  }

  DictionaryValue* initial_settings = maybe_initial_settings.GetSettings();
  for (DictionaryValue::key_iterator it = initial_settings->begin_keys();
      it != initial_settings->end_keys(); ++it) {
    Value *value;
    initial_settings->GetWithoutPathExpansion(*it, &value);
    Allocate(*it, *value, &used_total_, &used_per_setting_);
  }
}

ExtensionSettingsStorageQuotaEnforcer::~ExtensionSettingsStorageQuotaEnforcer(
    ) {}

ExtensionSettingsStorage::Result ExtensionSettingsStorageQuotaEnforcer::Get(
    const std::string& key) {
  return delegate_->Get(key);
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageQuotaEnforcer::Get(
    const std::vector<std::string>& keys) {
  return delegate_->Get(keys);
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageQuotaEnforcer::Get() {
  return delegate_->Get();
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageQuotaEnforcer::Set(
    const std::string& key, const Value& value) {
  size_t new_used_total = used_total_;
  std::map<std::string, size_t> new_used_per_setting = used_per_setting_;
  Allocate(key, value, &new_used_total, &new_used_per_setting);

  if (new_used_total > quota_bytes_ ||
      new_used_per_setting[key] > quota_bytes_per_setting_ ||
      new_used_per_setting.size() > max_keys_) {
    return Result(kExceededQuotaErrorMessage);
  }

  Result result = delegate_->Set(key, value);
  if (result.HasError()) {
    return result;
  }

  used_total_ = new_used_total;
  used_per_setting_.swap(new_used_per_setting);
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageQuotaEnforcer::Set(
    const DictionaryValue& values) {
  size_t new_used_total = used_total_;
  std::map<std::string, size_t> new_used_per_setting = used_per_setting_;
  for (DictionaryValue::key_iterator it = values.begin_keys();
      it != values.end_keys(); ++it) {
    Value* value;
    values.GetWithoutPathExpansion(*it, &value);
    Allocate(*it, *value, &new_used_total, &new_used_per_setting);

    if (new_used_per_setting[*it] > quota_bytes_per_setting_) {
      return Result(kExceededQuotaErrorMessage);
    }
  }

  if (new_used_total > quota_bytes_ ||
      new_used_per_setting.size() > max_keys_) {
    return Result(kExceededQuotaErrorMessage);
  }

  Result result = delegate_->Set(values);
  if (result.HasError()) {
    return result;
  }

  used_total_ = new_used_total;
  used_per_setting_ = new_used_per_setting;
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageQuotaEnforcer::Remove(
    const std::string& key) {
  Result result = delegate_->Remove(key);
  if (result.HasError()) {
    return result;
  }
  Free(&used_total_, &used_per_setting_, key);
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageQuotaEnforcer::Remove(
    const std::vector<std::string>& keys) {
  Result result = delegate_->Remove(keys);
  if (result.HasError()) {
    return result;
  }

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    Free(&used_total_, &used_per_setting_, *it);
  }
  return result;
}

ExtensionSettingsStorage::Result ExtensionSettingsStorageQuotaEnforcer::Clear(
    ) {
  Result result = delegate_->Clear();
  if (result.HasError()) {
    return result;
  }

  while (!used_per_setting_.empty()) {
    Free(&used_total_, &used_per_setting_, used_per_setting_.begin()->first);
  }
  return result;
}

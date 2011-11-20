// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_storage_quota_enforcer.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/task.h"

namespace extensions {

namespace {

const char* kExceededQuotaErrorMessage = "Quota exceeded";

// Resources there are a quota for.
enum Resource {
  TOTAL_BYTES,
  BYTES_PER_SETTING,
  KEY_COUNT
};

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

// Returns an error result and logs the quota exceeded to UMA.
SettingsStorage::WriteResult QuotaExceededFor(Resource resource) {
  switch (resource) {
    case TOTAL_BYTES:
      UMA_HISTOGRAM_COUNTS_100(
          "Extensions.SettingsQuotaExceeded.TotalBytes", 1);
      break;
    case BYTES_PER_SETTING:
      UMA_HISTOGRAM_COUNTS_100(
          "Extensions.SettingsQuotaExceeded.BytesPerSetting", 1);
      break;
    case KEY_COUNT:
      UMA_HISTOGRAM_COUNTS_100(
          "Extensions.SettingsQuotaExceeded.KeyCount", 1);
      break;
    default:
      NOTREACHED();
  }
  return SettingsStorage::WriteResult(kExceededQuotaErrorMessage);
}

}  // namespace

SettingsStorageQuotaEnforcer::SettingsStorageQuotaEnforcer(
    size_t quota_bytes,
    size_t quota_bytes_per_setting,
    size_t max_keys,
    SettingsStorage* delegate)
    : quota_bytes_(quota_bytes),
      quota_bytes_per_setting_(quota_bytes_per_setting),
      max_keys_(max_keys),
      delegate_(delegate),
      used_total_(0) {
  ReadResult maybe_settings = delegate->Get();
  if (maybe_settings.HasError()) {
    LOG(WARNING) << "Failed to get initial settings for quota: " <<
        maybe_settings.error();
    return;
  }

  for (DictionaryValue::Iterator it(maybe_settings.settings()); it.HasNext();
      it.Advance()) {
    Allocate(it.key(), it.value(), &used_total_, &used_per_setting_);
  }
}

SettingsStorageQuotaEnforcer::~SettingsStorageQuotaEnforcer() {}

SettingsStorage::ReadResult SettingsStorageQuotaEnforcer::Get(
    const std::string& key) {
  return delegate_->Get(key);
}

SettingsStorage::ReadResult SettingsStorageQuotaEnforcer::Get(
    const std::vector<std::string>& keys) {
  return delegate_->Get(keys);
}

SettingsStorage::ReadResult SettingsStorageQuotaEnforcer::Get() {
  return delegate_->Get();
}

SettingsStorage::WriteResult SettingsStorageQuotaEnforcer::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  size_t new_used_total = used_total_;
  std::map<std::string, size_t> new_used_per_setting = used_per_setting_;
  Allocate(key, value, &new_used_total, &new_used_per_setting);

  if (options != FORCE) {
    if (new_used_total > quota_bytes_) {
      return QuotaExceededFor(TOTAL_BYTES);
    }
    if (new_used_per_setting[key] > quota_bytes_per_setting_) {
      return QuotaExceededFor(BYTES_PER_SETTING);
    }
    if (new_used_per_setting.size() > max_keys_) {
      return QuotaExceededFor(KEY_COUNT);
    }
  }

  WriteResult result = delegate_->Set(options, key, value);
  if (result.HasError()) {
    return result;
  }

  used_total_ = new_used_total;
  used_per_setting_.swap(new_used_per_setting);
  return result;
}

SettingsStorage::WriteResult SettingsStorageQuotaEnforcer::Set(
    WriteOptions options, const DictionaryValue& values) {
  size_t new_used_total = used_total_;
  std::map<std::string, size_t> new_used_per_setting = used_per_setting_;
  for (DictionaryValue::Iterator it(values); it.HasNext(); it.Advance()) {
    Allocate(it.key(), it.value(), &new_used_total, &new_used_per_setting);

    if (options != FORCE &&
        new_used_per_setting[it.key()] > quota_bytes_per_setting_) {
      return QuotaExceededFor(BYTES_PER_SETTING);
    }
  }

  if (options != FORCE) {
    if (new_used_total > quota_bytes_) {
      return QuotaExceededFor(TOTAL_BYTES);
    }
    if (new_used_per_setting.size() > max_keys_) {
      return QuotaExceededFor(KEY_COUNT);
    }
  }

  WriteResult result = delegate_->Set(options, values);
  if (result.HasError()) {
    return result;
  }

  used_total_ = new_used_total;
  used_per_setting_ = new_used_per_setting;
  return result;
}

SettingsStorage::WriteResult SettingsStorageQuotaEnforcer::Remove(
    const std::string& key) {
  WriteResult result = delegate_->Remove(key);
  if (result.HasError()) {
    return result;
  }
  Free(&used_total_, &used_per_setting_, key);
  return result;
}

SettingsStorage::WriteResult SettingsStorageQuotaEnforcer::Remove(
    const std::vector<std::string>& keys) {
  WriteResult result = delegate_->Remove(keys);
  if (result.HasError()) {
    return result;
  }

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    Free(&used_total_, &used_per_setting_, *it);
  }
  return result;
}

SettingsStorage::WriteResult SettingsStorageQuotaEnforcer::Clear() {
  WriteResult result = delegate_->Clear();
  if (result.HasError()) {
    return result;
  }

  while (!used_per_setting_.empty()) {
    Free(&used_total_, &used_per_setting_, used_per_setting_.begin()->first);
  }
  return result;
}

}  // namespace extensions

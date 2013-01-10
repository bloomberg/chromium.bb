// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_storage_quota_enforcer.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "extensions/common/error_utils.h"

namespace extensions {

namespace {

const char* kQuotaExceededError = "* quota exceeded.";

// Resources there are a quota for.
enum Resource {
  QUOTA_BYTES,
  QUOTA_BYTES_PER_ITEM,
  MAX_ITEMS
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
  base::JSONWriter::Write(&value, &value_as_json);
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
ValueStore::WriteResult QuotaExceededFor(Resource resource) {
  std::string name;
  switch (resource) {
    case QUOTA_BYTES:
      name = "QUOTA_BYTES";
      UMA_HISTOGRAM_COUNTS_100(
          "Extensions.SettingsQuotaExceeded.TotalBytes", 1);
      break;
    case QUOTA_BYTES_PER_ITEM:
      name = "QUOTA_BYTES_PER_ITEM";
      UMA_HISTOGRAM_COUNTS_100(
          "Extensions.SettingsQuotaExceeded.BytesPerSetting", 1);
      break;
    case MAX_ITEMS:
      name = "MAX_ITEMS";
      UMA_HISTOGRAM_COUNTS_100(
          "Extensions.SettingsQuotaExceeded.KeyCount", 1);
      break;
    default:
      NOTREACHED();
  }
  return ValueStore::MakeWriteResult(
      ErrorUtils::FormatErrorMessage(kQuotaExceededError, name));
}

}  // namespace

SettingsStorageQuotaEnforcer::SettingsStorageQuotaEnforcer(
    const Limits& limits, ValueStore* delegate)
    : limits_(limits), delegate_(delegate), used_total_(0) {
  ReadResult maybe_settings = delegate_->Get();
  if (maybe_settings->HasError()) {
    LOG(WARNING) << "Failed to get initial settings for quota: " <<
        maybe_settings->error();
    return;
  }

  for (DictionaryValue::Iterator it(*maybe_settings->settings().get());
       it.HasNext(); it.Advance()) {
    Allocate(
        it.key(), it.value(), &used_total_, &used_per_setting_);
  }
}

SettingsStorageQuotaEnforcer::~SettingsStorageQuotaEnforcer() {}

size_t SettingsStorageQuotaEnforcer::GetBytesInUse(const std::string& key) {
  std::map<std::string, size_t>::iterator maybe_used =
      used_per_setting_.find(key);
  return maybe_used == used_per_setting_.end() ? 0u : maybe_used->second;
}

size_t SettingsStorageQuotaEnforcer::GetBytesInUse(
    const std::vector<std::string>& keys) {
  size_t used = 0;
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    used += GetBytesInUse(*it);
  }
  return used;
}

size_t SettingsStorageQuotaEnforcer::GetBytesInUse() {
  // All ValueStore implementations rely on GetBytesInUse being
  // implemented here.
  return used_total_;
}

ValueStore::ReadResult SettingsStorageQuotaEnforcer::Get(
    const std::string& key) {
  return delegate_->Get(key);
}

ValueStore::ReadResult SettingsStorageQuotaEnforcer::Get(
    const std::vector<std::string>& keys) {
  return delegate_->Get(keys);
}

ValueStore::ReadResult SettingsStorageQuotaEnforcer::Get() {
  return delegate_->Get();
}

ValueStore::WriteResult SettingsStorageQuotaEnforcer::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  size_t new_used_total = used_total_;
  std::map<std::string, size_t> new_used_per_setting = used_per_setting_;
  Allocate(key, value, &new_used_total, &new_used_per_setting);

  if (!(options & IGNORE_QUOTA)) {
    if (new_used_total > limits_.quota_bytes) {
      return QuotaExceededFor(QUOTA_BYTES);
    }
    if (new_used_per_setting[key] > limits_.quota_bytes_per_item) {
      return QuotaExceededFor(QUOTA_BYTES_PER_ITEM);
    }
    if (new_used_per_setting.size() > limits_.max_items) {
      return QuotaExceededFor(MAX_ITEMS);
    }
  }

  WriteResult result = delegate_->Set(options, key, value);
  if (result->HasError()) {
    return result.Pass();
  }

  used_total_ = new_used_total;
  used_per_setting_.swap(new_used_per_setting);
  return result.Pass();
}

ValueStore::WriteResult SettingsStorageQuotaEnforcer::Set(
    WriteOptions options, const DictionaryValue& values) {
  size_t new_used_total = used_total_;
  std::map<std::string, size_t> new_used_per_setting = used_per_setting_;
  for (DictionaryValue::Iterator it(values); it.HasNext(); it.Advance()) {
    Allocate(it.key(), it.value(), &new_used_total, &new_used_per_setting);

    if (!(options & IGNORE_QUOTA) &&
        new_used_per_setting[it.key()] > limits_.quota_bytes_per_item) {
      return QuotaExceededFor(QUOTA_BYTES_PER_ITEM);
    }
  }

  if (!(options & IGNORE_QUOTA)) {
    if (new_used_total > limits_.quota_bytes) {
      return QuotaExceededFor(QUOTA_BYTES);
    }
    if (new_used_per_setting.size() > limits_.max_items) {
      return QuotaExceededFor(MAX_ITEMS);
    }
  }

  WriteResult result = delegate_->Set(options, values);
  if (result->HasError()) {
    return result.Pass();
  }

  used_total_ = new_used_total;
  used_per_setting_ = new_used_per_setting;
  return result.Pass();
}

ValueStore::WriteResult SettingsStorageQuotaEnforcer::Remove(
    const std::string& key) {
  WriteResult result = delegate_->Remove(key);
  if (result->HasError()) {
    return result.Pass();
  }
  Free(&used_total_, &used_per_setting_, key);
  return result.Pass();
}

ValueStore::WriteResult SettingsStorageQuotaEnforcer::Remove(
    const std::vector<std::string>& keys) {
  WriteResult result = delegate_->Remove(keys);
  if (result->HasError()) {
    return result.Pass();
  }

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    Free(&used_total_, &used_per_setting_, *it);
  }
  return result.Pass();
}

ValueStore::WriteResult SettingsStorageQuotaEnforcer::Clear() {
  WriteResult result = delegate_->Clear();
  if (result->HasError()) {
    return result.Pass();
  }

  while (!used_per_setting_.empty()) {
    Free(&used_total_, &used_per_setting_, used_per_setting_.begin()->first);
  }
  return result.Pass();
}

}  // namespace extensions

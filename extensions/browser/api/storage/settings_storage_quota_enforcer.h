// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_STORAGE_QUOTA_ENFORCER_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_STORAGE_QUOTA_ENFORCER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/value_store/value_store.h"

namespace extensions {

// Enforces total quota and a per-setting quota in bytes, and a maximum number
// of setting keys, for a delegate storage area.
class SettingsStorageQuotaEnforcer : public ValueStore {
 public:
  struct Limits {
    // The total quota in bytes.
    size_t quota_bytes;

    // The quota for each individual item in bytes.
    size_t quota_bytes_per_item;

    // The maximum number of items allowed.
    size_t max_items;
  };

  SettingsStorageQuotaEnforcer(const Limits& limits, ValueStore* delegate);

  virtual ~SettingsStorageQuotaEnforcer();

  // ValueStore implementation.
  virtual size_t GetBytesInUse(const std::string& key) OVERRIDE;
  virtual size_t GetBytesInUse(const std::vector<std::string>& keys) OVERRIDE;
  virtual size_t GetBytesInUse() OVERRIDE;
  virtual ReadResult Get(const std::string& key) OVERRIDE;
  virtual ReadResult Get(const std::vector<std::string>& keys) OVERRIDE;
  virtual ReadResult Get() OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options,
      const std::string& key,
      const base::Value& value) OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options, const base::DictionaryValue& values) OVERRIDE;
  virtual WriteResult Remove(const std::string& key) OVERRIDE;
  virtual WriteResult Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual WriteResult Clear() OVERRIDE;
  virtual bool Restore() OVERRIDE;
  virtual bool RestoreKey(const std::string& key) OVERRIDE;

  ValueStore* get_delegate_for_test() { return delegate_.get(); }

 private:
  // Calculate the current usage for the database.
  void CalculateUsage();

  // Limits configuration.
  const Limits limits_;

  // The delegate storage area.
  scoped_ptr<ValueStore> const delegate_;

  // Total bytes in used by |delegate_|. Includes both key lengths and
  // JSON-encoded values.
  size_t used_total_;

  // Map of item key to its size, including the key itself.
  std::map<std::string, size_t> used_per_setting_;

  DISALLOW_COPY_AND_ASSIGN(SettingsStorageQuotaEnforcer);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_STORAGE_QUOTA_ENFORCER_H_

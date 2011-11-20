// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_QUOTA_ENFORCER_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_QUOTA_ENFORCER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/settings/settings_storage.h"

namespace extensions {

// Enforces total quota and a per-setting quota in bytes, and a maximum number
// of setting keys, for a delegate storage area.
class SettingsStorageQuotaEnforcer : public SettingsStorage {
 public:
  SettingsStorageQuotaEnforcer(
      size_t quota_bytes,
      size_t quota_bytes_per_setting,
      size_t max_keys,
      // Ownership taken.
      SettingsStorage* delegate);

  virtual ~SettingsStorageQuotaEnforcer();

  // SettingsStorage implementation.
  virtual ReadResult Get(const std::string& key) OVERRIDE;
  virtual ReadResult Get(const std::vector<std::string>& keys) OVERRIDE;
  virtual ReadResult Get() OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options,
      const std::string& key,
      const Value& value) OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options, const DictionaryValue& values) OVERRIDE;
  virtual WriteResult Remove(const std::string& key) OVERRIDE;
  virtual WriteResult Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual WriteResult Clear() OVERRIDE;

 private:
  // The storage quota in bytes.
  size_t const quota_bytes_;

  // The quota per individual setting in bytes.
  size_t const quota_bytes_per_setting_;

  // The maximum number of settings keys allowed.
  size_t const max_keys_;

  // The delegate storage area.
  scoped_ptr<SettingsStorage> const delegate_;

  // Total size of the settings currently being used.  Includes both settings
  // keys and their JSON-encoded values.
  size_t used_total_;

  // Map of key to size of that key, including the key itself.
  std::map<std::string, size_t> used_per_setting_;

  DISALLOW_COPY_AND_ASSIGN(SettingsStorageQuotaEnforcer);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_QUOTA_ENFORCER_H_

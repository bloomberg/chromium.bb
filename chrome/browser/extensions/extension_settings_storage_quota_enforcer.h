// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_QUOTA_ENFORCER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_QUOTA_ENFORCER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// Enforces a quota size in bytes and a maximum number of setting keys for a
// delegate storage area.
class ExtensionSettingsStorageQuotaEnforcer : public ExtensionSettingsStorage {
 public:
  ExtensionSettingsStorageQuotaEnforcer(
      size_t quota_bytes,
      size_t max_keys,
      // Ownership taken.
      ExtensionSettingsStorage* delegate);

  virtual ~ExtensionSettingsStorageQuotaEnforcer();

  // ExtensionSettingsStorage implementation.
  virtual Result Get(const std::string& key) OVERRIDE;
  virtual Result Get(const std::vector<std::string>& keys) OVERRIDE;
  virtual Result Get() OVERRIDE;
  virtual Result Set(const std::string& key, const Value& value) OVERRIDE;
  virtual Result Set(const DictionaryValue& settings) OVERRIDE;
  virtual Result Remove(const std::string& key) OVERRIDE;
  virtual Result Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual Result Clear() OVERRIDE;

 private:
  // The storage quota in bytes.
  size_t const quota_bytes_;

  // The maximum number of settings keys allowed.
  size_t const max_keys_;

  // The delegate storage area.
  scoped_ptr<ExtensionSettingsStorage> const delegate_;

  // Total size of the settings currently being used.  Includes both settings
  // keys and their JSON-encoded values.
  size_t used_total_;

  // Map of key to size of that key, including the key itself.
  std::map<std::string, size_t> used_per_setting_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsStorageQuotaEnforcer);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_QUOTA_ENFORCER_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_CACHE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/settings/settings_storage.h"

// Wraps a storage area with a cache.  Ownership of the delegate storage
// will be taken by the cache.
//
namespace extensions {

// Calls to Get() will return from the cache if an entry is present, or be
// passed to the delegate and the result cached if not.
// Calls to Set() / Clear() / Remove() are written through to the delegate,
// then stored in the cache if successful.
class SettingsStorageCache : public SettingsStorage {
 public:
  // Ownership of delegate taken.
  explicit SettingsStorageCache(SettingsStorage* delegate);
  virtual ~SettingsStorageCache();

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
  // Returns whether the value was found in the cache.
  // Ownership of value is released to the caller and placed in value.
  bool GetFromCache(const std::string& key, Value** value);

  // Storage that the cache is wrapping.
  scoped_ptr<SettingsStorage> delegate_;

  // The in-memory cache of settings from the delegate.
  DictionaryValue cache_;

  DISALLOW_COPY_AND_ASSIGN(SettingsStorageCache);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_CACHE_H_

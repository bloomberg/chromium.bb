// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_CACHE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// Wraps a storage area with a cache.  Ownership of the delegate storage
// will be taken by the cache.
//
// Calls to Get() will return from the cache if an entry is present, or be
// passed to the delegate and the result cached if not.
// Calls to Set() / Clear() / Remove() are written through to the delegate,
// then stored in the cache if successful.
class ExtensionSettingsStorageCache : public ExtensionSettingsStorage {
 public:
  // Ownership of delegate taken.
  explicit ExtensionSettingsStorageCache(ExtensionSettingsStorage* delegate);
  virtual ~ExtensionSettingsStorageCache();

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
  // Returns whether the value was found in the cache.
  // Ownership of value is released to the caller and placed in value.
  bool GetFromCache(const std::string& key, Value** value);

  // Storage that the cache is wrapping.
  scoped_ptr<ExtensionSettingsStorage> delegate_;

  // The in-memory cache of settings from the delegate.
  DictionaryValue cache_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsStorageCache);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_CACHE_H_

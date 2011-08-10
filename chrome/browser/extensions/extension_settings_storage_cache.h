// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_CACHE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// Wraps a storage area with a cache.  Ownership of the delegate storage
// will be taken by the cache.
//
// Calls to Get() will return from the cache if an entry is present, or be
// passed to the delegate and the result cached if not.
//
// Calls to Set() / Clear() / Remove() are written through to the delegate,
// then stored in the cache if successful.
class ExtensionSettingsStorageCache : public ExtensionSettingsStorage {
 public:
  // Creates a cache around a delegate.  Ownership of the delegate is taken.
  explicit ExtensionSettingsStorageCache(ExtensionSettingsStorage* delegate);

  virtual void DeleteSoon() OVERRIDE;

  virtual void Get(const std::string& key, Callback* callback) OVERRIDE;
  virtual void Get(const ListValue& keys, Callback* callback) OVERRIDE;
  virtual void Get(Callback* callback) OVERRIDE;
  virtual void Set(
      const std::string& key, const Value& value, Callback* callback) OVERRIDE;
  virtual void Set(const DictionaryValue& values, Callback* callback) OVERRIDE;
  virtual void Remove(const std::string& key, Callback* callback) OVERRIDE;
  virtual void Remove(const ListValue& keys, Callback* callback) OVERRIDE;
  virtual void Clear(Callback *callback) OVERRIDE;

 private:
  // Delete with DeleteSoon().
  virtual ~ExtensionSettingsStorageCache();

  // Returns whether the value was found in the cache.
  // Ownership of value is released to the caller and placed in value.
  bool GetFromCache(const std::string& key, Value** value);

  // Deleted in DeleteSoon().
  ExtensionSettingsStorage* delegate_;
  DictionaryValue cache_;
  base::WeakPtrFactory<DictionaryValue> cache_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsStorageCache);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_STORAGE_CACHE_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_NOOP_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_NOOP_STORAGE_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// Implementation of ExtensionSettingsStorage which does nothing, but behaves
// as though each Get/Set were successful.
// Intended to be wrapped by a Cache for trivial in-memory storage.
class ExtensionSettingsNoopStorage : public ExtensionSettingsStorage {
 public:
  ExtensionSettingsNoopStorage() {}

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
  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsNoopStorage);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_NOOP_STORAGE_H_

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

  // ExtensionSettingsStorage implementation.
  virtual Result Get(const std::string& key) OVERRIDE;
  virtual Result Get(const std::vector<std::string>& keys) OVERRIDE;
  virtual Result Get() OVERRIDE;
  virtual Result Set(const std::string& key, const Value& value) OVERRIDE;
  virtual Result Set(const DictionaryValue& values) OVERRIDE;
  virtual Result Remove(const std::string& key) OVERRIDE;
  virtual Result Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual Result Clear() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsNoopStorage);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_NOOP_STORAGE_H_

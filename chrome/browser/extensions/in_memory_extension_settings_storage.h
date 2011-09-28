// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_IN_MEMORY_EXTENSION_SETTINGS_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_IN_MEMORY_EXTENSION_SETTINGS_STORAGE_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// In-memory storage, as opposed to ExtensionSettingsLeveldbStorage.
class InMemoryExtensionSettingsStorage : public ExtensionSettingsStorage {
 public:
  InMemoryExtensionSettingsStorage() {}

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
  DictionaryValue storage_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryExtensionSettingsStorage);
};

#endif  // CHROME_BROWSER_EXTENSIONS_IN_MEMORY_EXTENSION_SETTINGS_STORAGE_H_

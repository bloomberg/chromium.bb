// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_FACTORY_H_
#pragma once

class FilePath;

namespace extensions {

class SettingsStorage;

// Factory for creating SettingStorage instances.
class SettingsStorageFactory {
 public:
  virtual ~SettingsStorageFactory() {}

  // Create a new SettingsLeveldbStorage area.  Return NULL to indicate
  // failure.  Must be called on the FILE thread.
  virtual SettingsStorage* Create(
      const FilePath& base_path, const std::string& extension_id) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_STORAGE_FACTORY_H_

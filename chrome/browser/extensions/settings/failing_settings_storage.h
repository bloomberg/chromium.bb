// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_FAILING_SETTINGS_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_FAILING_SETTINGS_STORAGE_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/settings/settings_storage.h"

namespace extensions {

// Settings storage area which fails every request.
class FailingSettingsStorage : public SettingsStorage {
 public:
  FailingSettingsStorage() {}

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
  DISALLOW_COPY_AND_ASSIGN(FailingSettingsStorage);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_FAILING_SETTINGS_STORAGE_H_

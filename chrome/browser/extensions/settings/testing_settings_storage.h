// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_TESTING_SETTINGS_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_TESTING_SETTINGS_STORAGE_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/settings/settings_storage.h"

namespace extensions {

// SettingsStorage for testing, with an in-memory storage but the ability to
// optionally fail all operations.
class TestingSettingsStorage : public SettingsStorage {
 public:
  TestingSettingsStorage();
  virtual ~TestingSettingsStorage();

  // Sets whether to fail all requests (default is false).
  void SetFailAllRequests(bool fail_all_requests);

  // SettingsStorage implementation.
  virtual ReadResult Get(const std::string& key) OVERRIDE;
  virtual ReadResult Get(const std::vector<std::string>& keys) OVERRIDE;
  virtual ReadResult Get() OVERRIDE;
  virtual WriteResult Set(const std::string& key, const Value& value) OVERRIDE;
  virtual WriteResult Set(const DictionaryValue& values) OVERRIDE;
  virtual WriteResult Remove(const std::string& key) OVERRIDE;
  virtual WriteResult Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual WriteResult Clear() OVERRIDE;

 private:
  DictionaryValue storage_;

  bool fail_all_requests_;

  DISALLOW_COPY_AND_ASSIGN(TestingSettingsStorage);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_TESTING_SETTINGS_STORAGE_H_

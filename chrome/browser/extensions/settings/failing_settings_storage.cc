// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/failing_settings_storage.h"

namespace extensions {

namespace {

const char* kGenericErrorMessage = "Failed to initialize settings";

SettingsStorage::ReadResult ReadResultError() {
  return SettingsStorage::ReadResult(kGenericErrorMessage);
}

SettingsStorage::WriteResult WriteResultError() {
  return SettingsStorage::WriteResult(kGenericErrorMessage);
}

}  // namespace

SettingsStorage::ReadResult FailingSettingsStorage::Get(
    const std::string& key) {
  return ReadResultError();
}

SettingsStorage::ReadResult FailingSettingsStorage::Get(
    const std::vector<std::string>& keys) {
  return ReadResultError();
}

SettingsStorage::ReadResult FailingSettingsStorage::Get() {
  return ReadResultError();
}

SettingsStorage::WriteResult FailingSettingsStorage::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  return WriteResultError();
}

SettingsStorage::WriteResult FailingSettingsStorage::Set(
    WriteOptions options, const DictionaryValue& settings) {
  return WriteResultError();
}

SettingsStorage::WriteResult FailingSettingsStorage::Remove(
    const std::string& key) {
  return WriteResultError();
}

SettingsStorage::WriteResult FailingSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  return WriteResultError();
}

SettingsStorage::WriteResult FailingSettingsStorage::Clear() {
  return WriteResultError();
}

}  // namespace extensions

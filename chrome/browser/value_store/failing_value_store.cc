// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/failing_value_store.h"

#include "base/logging.h"

namespace {

const char* kGenericErrorMessage = "Failed to initialize settings";

ValueStore::ReadResult ReadResultError() {
  return ValueStore::ReadResult(kGenericErrorMessage);
}

ValueStore::WriteResult WriteResultError() {
  return ValueStore::WriteResult(kGenericErrorMessage);
}

}  // namespace

size_t FailingSettingsStorage::GetBytesInUse(const std::string& key) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

size_t FailingSettingsStorage::GetBytesInUse(
    const std::vector<std::string>& keys) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

size_t FailingSettingsStorage::GetBytesInUse() {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

ValueStore::ReadResult FailingSettingsStorage::Get(
    const std::string& key) {
  return ReadResultError();
}

ValueStore::ReadResult FailingSettingsStorage::Get(
    const std::vector<std::string>& keys) {
  return ReadResultError();
}

ValueStore::ReadResult FailingSettingsStorage::Get() {
  return ReadResultError();
}

ValueStore::WriteResult FailingSettingsStorage::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  return WriteResultError();
}

ValueStore::WriteResult FailingSettingsStorage::Set(
    WriteOptions options, const DictionaryValue& settings) {
  return WriteResultError();
}

ValueStore::WriteResult FailingSettingsStorage::Remove(
    const std::string& key) {
  return WriteResultError();
}

ValueStore::WriteResult FailingSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  return WriteResultError();
}

ValueStore::WriteResult FailingSettingsStorage::Clear() {
  return WriteResultError();
}

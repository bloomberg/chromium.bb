// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/failing_value_store.h"

#include "base/logging.h"

namespace {

const char* kGenericErrorMessage = "Failed to initialize settings";

ValueStore::ReadResult ReadResultError() {
  return ValueStore::MakeReadResult(kGenericErrorMessage);
}

ValueStore::WriteResult WriteResultError() {
  return ValueStore::MakeWriteResult(kGenericErrorMessage);
}

}  // namespace

size_t FailingValueStore::GetBytesInUse(const std::string& key) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

size_t FailingValueStore::GetBytesInUse(
    const std::vector<std::string>& keys) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

size_t FailingValueStore::GetBytesInUse() {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

ValueStore::ReadResult FailingValueStore::Get(
    const std::string& key) {
  return ReadResultError();
}

ValueStore::ReadResult FailingValueStore::Get(
    const std::vector<std::string>& keys) {
  return ReadResultError();
}

ValueStore::ReadResult FailingValueStore::Get() {
  return ReadResultError();
}

ValueStore::WriteResult FailingValueStore::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  return WriteResultError();
}

ValueStore::WriteResult FailingValueStore::Set(
    WriteOptions options, const DictionaryValue& settings) {
  return WriteResultError();
}

ValueStore::WriteResult FailingValueStore::Remove(
    const std::string& key) {
  return WriteResultError();
}

ValueStore::WriteResult FailingValueStore::Remove(
    const std::vector<std::string>& keys) {
  return WriteResultError();
}

ValueStore::WriteResult FailingValueStore::Clear() {
  return WriteResultError();
}

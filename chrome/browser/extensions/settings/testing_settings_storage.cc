// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/testing_settings_storage.h"

#include "base/logging.h"

namespace extensions {

namespace {

const char* kGenericErrorMessage = "TestingSettingsStorage configured to error";

SettingsStorage::ReadResult ReadResultError() {
  return SettingsStorage::ReadResult(kGenericErrorMessage);
}

SettingsStorage::WriteResult WriteResultError() {
  return SettingsStorage::WriteResult(kGenericErrorMessage);
}

std::vector<std::string> CreateVector(const std::string& string) {
  std::vector<std::string> strings;
  strings.push_back(string);
  return strings;
}

}  // namespace

TestingSettingsStorage::TestingSettingsStorage()
    : fail_all_requests_(false) {}

TestingSettingsStorage::~TestingSettingsStorage() {}

void TestingSettingsStorage::SetFailAllRequests(bool fail_all_requests) {
  fail_all_requests_ = fail_all_requests;
}

SettingsStorage::ReadResult TestingSettingsStorage::Get(
    const std::string& key) {
  return Get(CreateVector(key));
}

SettingsStorage::ReadResult TestingSettingsStorage::Get(
    const std::vector<std::string>& keys) {
  if (fail_all_requests_) {
    return ReadResultError();
  }

  DictionaryValue* settings = new DictionaryValue();
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    Value* value = NULL;
    if (storage_.GetWithoutPathExpansion(*it, &value)) {
      settings->SetWithoutPathExpansion(*it, value->DeepCopy());
    }
  }
  return ReadResult(settings);
}

SettingsStorage::ReadResult TestingSettingsStorage::Get() {
  if (fail_all_requests_) {
    return ReadResultError();
  }
  return ReadResult(storage_.DeepCopy());
}

SettingsStorage::WriteResult TestingSettingsStorage::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  DictionaryValue settings;
  settings.SetWithoutPathExpansion(key, value.DeepCopy());
  return Set(options, settings);
}

SettingsStorage::WriteResult TestingSettingsStorage::Set(
    WriteOptions options, const DictionaryValue& settings) {
  if (fail_all_requests_) {
    return WriteResultError();
  }

  scoped_ptr<SettingChangeList> changes(new SettingChangeList());
  for (DictionaryValue::Iterator it(settings); it.HasNext(); it.Advance()) {
    Value* old_value = NULL;
    storage_.GetWithoutPathExpansion(it.key(), &old_value);
    if (!old_value || !old_value->Equals(&it.value())) {
      changes->push_back(
          SettingChange(
              it.key(),
              old_value ? old_value->DeepCopy() : old_value,
              it.value().DeepCopy()));
      storage_.SetWithoutPathExpansion(it.key(), it.value().DeepCopy());
    }
  }
  return WriteResult(changes.release());
}

SettingsStorage::WriteResult TestingSettingsStorage::Remove(
    const std::string& key) {
  return Remove(CreateVector(key));
}

SettingsStorage::WriteResult TestingSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  if (fail_all_requests_) {
    return WriteResultError();
  }

  scoped_ptr<SettingChangeList> changes(
      new SettingChangeList());
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    Value* old_value = NULL;
    if (storage_.RemoveWithoutPathExpansion(*it, &old_value)) {
      changes->push_back(SettingChange(*it, old_value, NULL));
    }
  }
  return WriteResult(changes.release());
}

SettingsStorage::WriteResult TestingSettingsStorage::Clear() {
  if (fail_all_requests_) {
    return WriteResultError();
  }

  std::vector<std::string> keys;
  for (DictionaryValue::Iterator it(storage_); it.HasNext(); it.Advance()) {
    keys.push_back(it.key());
  }
  return Remove(keys);
}

}  // namespace extensions

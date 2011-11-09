// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/in_memory_settings_storage.h"

#include "base/logging.h"

namespace extensions {

namespace {

std::vector<std::string> CreateVector(const std::string& string) {
  std::vector<std::string> strings;
  strings.push_back(string);
  return strings;
}

}  // namespace

SettingsStorage::ReadResult InMemorySettingsStorage::Get(
    const std::string& key) {
  return Get(CreateVector(key));
}

SettingsStorage::ReadResult InMemorySettingsStorage::Get(
    const std::vector<std::string>& keys) {
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

SettingsStorage::ReadResult InMemorySettingsStorage::Get() {
  return ReadResult(storage_.DeepCopy());
}

SettingsStorage::WriteResult InMemorySettingsStorage::Set(
    const std::string& key, const Value& value) {
  DictionaryValue settings;
  settings.SetWithoutPathExpansion(key, value.DeepCopy());
  return Set(settings);
}

SettingsStorage::WriteResult InMemorySettingsStorage::Set(
    const DictionaryValue& settings) {
  scoped_ptr<SettingChangeList> changes(
      new SettingChangeList());
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

SettingsStorage::WriteResult InMemorySettingsStorage::Remove(
    const std::string& key) {
  return Remove(CreateVector(key));
}

SettingsStorage::WriteResult InMemorySettingsStorage::Remove(
    const std::vector<std::string>& keys) {
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

SettingsStorage::WriteResult
InMemorySettingsStorage::Clear() {
  std::vector<std::string> keys;
  for (DictionaryValue::Iterator it(storage_); it.HasNext(); it.Advance()) {
    keys.push_back(it.key());
  }
  return Remove(keys);
}

}  // namespace extensions

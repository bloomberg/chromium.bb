// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/in_memory_extension_settings_storage.h"

#include "base/logging.h"

namespace {

std::vector<std::string> CreateVector(const std::string& string) {
  std::vector<std::string> strings;
  strings.push_back(string);
  return strings;
}

}  // namespace

ExtensionSettingsStorage::ReadResult InMemoryExtensionSettingsStorage::Get(
    const std::string& key) {
  return Get(CreateVector(key));
}

ExtensionSettingsStorage::ReadResult InMemoryExtensionSettingsStorage::Get(
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

ExtensionSettingsStorage::ReadResult InMemoryExtensionSettingsStorage::Get() {
  return ReadResult(storage_.DeepCopy());
}

ExtensionSettingsStorage::WriteResult InMemoryExtensionSettingsStorage::Set(
    const std::string& key, const Value& value) {
  DictionaryValue settings;
  settings.SetWithoutPathExpansion(key, value.DeepCopy());
  return Set(settings);
}

ExtensionSettingsStorage::WriteResult InMemoryExtensionSettingsStorage::Set(
    const DictionaryValue& settings) {
  scoped_ptr<ExtensionSettingChangeList> changes(
      new ExtensionSettingChangeList());
  for (DictionaryValue::Iterator it(settings); it.HasNext(); it.Advance()) {
    Value* old_value = NULL;
    storage_.GetWithoutPathExpansion(it.key(), &old_value);
    if (!old_value || !old_value->Equals(&it.value())) {
      changes->push_back(
          ExtensionSettingChange(
              it.key(),
              old_value ? old_value->DeepCopy() : old_value,
              it.value().DeepCopy()));
      storage_.SetWithoutPathExpansion(it.key(), it.value().DeepCopy());
    }
  }
  return WriteResult(changes.release());
}

ExtensionSettingsStorage::WriteResult InMemoryExtensionSettingsStorage::Remove(
    const std::string& key) {
  return Remove(CreateVector(key));
}

ExtensionSettingsStorage::WriteResult InMemoryExtensionSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  scoped_ptr<ExtensionSettingChangeList> changes(
      new ExtensionSettingChangeList());
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    Value* old_value = NULL;
    if (storage_.RemoveWithoutPathExpansion(*it, &old_value)) {
      changes->push_back(ExtensionSettingChange(*it, old_value, NULL));
    }
  }
  return WriteResult(changes.release());
}

ExtensionSettingsStorage::WriteResult
InMemoryExtensionSettingsStorage::Clear() {
  std::vector<std::string> keys;
  for (DictionaryValue::Iterator it(storage_); it.HasNext(); it.Advance()) {
    keys.push_back(it.key());
  }
  return Remove(keys);
}

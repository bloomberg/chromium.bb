// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/in_memory_extension_settings_storage.h"

namespace {

std::vector<std::string> CreateVector(const std::string& string) {
  std::vector<std::string> strings;
  strings.push_back(string);
  return strings;
}

}  // namespace

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Get(
    const std::string& key) {
  return Get(CreateVector(key));
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Get(
    const std::vector<std::string>& keys) {
  DictionaryValue* settings = new DictionaryValue();
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    Value* value = NULL;
    if (storage_.GetWithoutPathExpansion(*it, &value)) {
      settings->SetWithoutPathExpansion(*it, value->DeepCopy());
    }
  }
  return Result(settings, NULL);
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Get() {
  return Result(storage_.DeepCopy(), NULL);
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Set(
    const std::string& key, const Value& value) {
  DictionaryValue settings;
  settings.SetWithoutPathExpansion(key, value.DeepCopy());
  return Set(settings);
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Set(
    const DictionaryValue& settings) {
  std::set<std::string>* changed_keys = new std::set<std::string>();
  for (DictionaryValue::key_iterator it = settings.begin_keys();
      it != settings.end_keys(); ++it) {
    Value* old_value = NULL;
    storage_.GetWithoutPathExpansion(*it, &old_value);
    Value* new_value = NULL;
    settings.GetWithoutPathExpansion(*it, &new_value);
    if (old_value == NULL || !old_value->Equals(new_value)) {
      changed_keys->insert(*it);
      storage_.SetWithoutPathExpansion(*it, new_value->DeepCopy());
    }
  }
  return Result(settings.DeepCopy(), changed_keys);
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Remove(
    const std::string& key) {
  return Remove(CreateVector(key));
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  std::set<std::string>* changed_keys = new std::set<std::string>();
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    if (storage_.RemoveWithoutPathExpansion(*it, NULL)) {
      changed_keys->insert(*it);
    }
  }
  return Result(NULL, changed_keys);
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Clear() {
  std::set<std::string>* changed_keys = new std::set<std::string>();
  for (DictionaryValue::key_iterator it = storage_.begin_keys();
      it != storage_.end_keys(); ++it) {
    changed_keys->insert(*it);
  }
  storage_.Clear();
  return Result(NULL, changed_keys);
}

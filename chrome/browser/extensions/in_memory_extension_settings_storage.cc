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
  return Result(settings);
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Get() {
  return Result(storage_.DeepCopy());
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Set(
    const std::string& key, const Value& value) {
  DictionaryValue settings;
  settings.SetWithoutPathExpansion(key, value.DeepCopy());
  return Set(settings);
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Set(
    const DictionaryValue& settings) {
  storage_.MergeDictionary(&settings);
  return Result(settings.DeepCopy());
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Remove(
    const std::string& key) {
  return Remove(CreateVector(key));
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    storage_.RemoveWithoutPathExpansion(*it, NULL);
  }
  return Result(NULL);
}

ExtensionSettingsStorage::Result InMemoryExtensionSettingsStorage::Clear() {
  storage_.Clear();
  return Result(NULL);
}

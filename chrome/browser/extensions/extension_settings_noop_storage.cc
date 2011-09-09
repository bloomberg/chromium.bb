// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_noop_storage.h"

ExtensionSettingsStorage::Result ExtensionSettingsNoopStorage::Get(
    const std::string& key) {
  return Result(new DictionaryValue());
}

ExtensionSettingsStorage::Result ExtensionSettingsNoopStorage::Get(
    const std::vector<std::string>& keys) {
  return Result(new DictionaryValue());
}

ExtensionSettingsStorage::Result ExtensionSettingsNoopStorage::Get() {
  return Result(new DictionaryValue());
}

ExtensionSettingsStorage::Result ExtensionSettingsNoopStorage::Set(
    const std::string& key, const Value& value) {
  DictionaryValue* settings = new DictionaryValue();
  settings->SetWithoutPathExpansion(key, value.DeepCopy());
  return Result(settings);
}

ExtensionSettingsStorage::Result ExtensionSettingsNoopStorage::Set(
    const DictionaryValue& settings) {
  return Result(settings.DeepCopy());
}

ExtensionSettingsStorage::Result ExtensionSettingsNoopStorage::Remove(
    const std::string& key) {
  return Result(NULL);
}

ExtensionSettingsStorage::Result ExtensionSettingsNoopStorage::Remove(
    const std::vector<std::string>& keys) {
  return Result(NULL);
}

ExtensionSettingsStorage::Result ExtensionSettingsNoopStorage::Clear() {
  return Result(NULL);
}

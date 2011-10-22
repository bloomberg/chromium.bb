// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_setting_changes.h"

#include "base/json/json_writer.h"

void ExtensionSettingChanges::Builder::AppendChange(
    const std::string& key, Value* old_value, Value* new_value) {
  DictionaryValue* change = new DictionaryValue();
  changes_.Append(change);

  change->SetString("key", key);
  if (old_value) {
    change->Set("oldValue", old_value);
  }
  if (new_value) {
    change->Set("newValue", new_value);
  }
}

ExtensionSettingChanges ExtensionSettingChanges::Builder::Build() {
  return ExtensionSettingChanges(&changes_);
}

ExtensionSettingChanges::ExtensionSettingChanges(ListValue* changes)
    : inner_(new Inner()) {
  inner_->changes_.Swap(changes);
}

ExtensionSettingChanges::~ExtensionSettingChanges() {}

std::string ExtensionSettingChanges::ToJson() const {
  std::string changes_json;
  base::JSONWriter::Write(&inner_->changes_, false, &changes_json);
  return changes_json;
}

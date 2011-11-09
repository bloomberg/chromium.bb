// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_setting_change.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"

/* static */
std::string ExtensionSettingChange::GetEventJson(
    const ExtensionSettingChangeList& changes) {
  ListValue changes_value;
  for (ExtensionSettingChangeList::const_iterator it = changes.begin();
      it != changes.end(); ++it) {
    DictionaryValue* change_value = new DictionaryValue();
    change_value->SetString("key", it->key());
    if (it->old_value()) {
      change_value->Set("oldValue", it->old_value()->DeepCopy());
    }
    if (it->new_value()) {
      change_value->Set("newValue", it->new_value()->DeepCopy());
    }
    changes_value.Append(change_value);
  }
  std::string json;
  base::JSONWriter::Write(&changes_value, false, &json);
  return json;
}

ExtensionSettingChange::ExtensionSettingChange(
    const std::string& key, Value* old_value, Value* new_value)
    : inner_(new Inner(key, old_value, new_value)) {}

ExtensionSettingChange::~ExtensionSettingChange() {}

const std::string& ExtensionSettingChange::key() const {
  DCHECK(inner_.get());
  return inner_->key_;
}

const Value* ExtensionSettingChange::old_value() const {
  DCHECK(inner_.get());
  return inner_->old_value_.get();
}

const Value* ExtensionSettingChange::new_value() const {
  DCHECK(inner_.get());
  return inner_->new_value_.get();
}

ExtensionSettingChange::Inner::Inner(
    const std::string& key, base::Value* old_value, base::Value* new_value)
    : key_(key), old_value_(old_value), new_value_(new_value) {}

ExtensionSettingChange::Inner::~Inner() {}

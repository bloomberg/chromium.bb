// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/setting_change.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"

namespace extensions {

/* static */
std::string SettingChange::GetEventJson(const SettingChangeList& changes) {
  DictionaryValue changes_value;
  for (SettingChangeList::const_iterator it = changes.begin();
      it != changes.end(); ++it) {
    DictionaryValue* change_value = new DictionaryValue();
    if (it->old_value()) {
      change_value->Set("oldValue", it->old_value()->DeepCopy());
    }
    if (it->new_value()) {
      change_value->Set("newValue", it->new_value()->DeepCopy());
    }
    changes_value.Set(it->key(), change_value);
  }
  std::string json;
  base::JSONWriter::Write(&changes_value, false, &json);
  return json;
}

SettingChange::SettingChange(
    const std::string& key, Value* old_value, Value* new_value)
    : inner_(new Inner(key, old_value, new_value)) {}

SettingChange::~SettingChange() {}

const std::string& SettingChange::key() const {
  DCHECK(inner_.get());
  return inner_->key_;
}

const Value* SettingChange::old_value() const {
  DCHECK(inner_.get());
  return inner_->old_value_.get();
}

const Value* SettingChange::new_value() const {
  DCHECK(inner_.get());
  return inner_->new_value_.get();
}

SettingChange::Inner::Inner(
    const std::string& key, base::Value* old_value, base::Value* new_value)
    : key_(key), old_value_(old_value), new_value_(new_value) {}

SettingChange::Inner::~Inner() {}

}  // namespace extensions

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// Implementation of ExtensionSettingsStorage::Result

ExtensionSettingsStorage::Result::Result(
    DictionaryValue* settings,
    DictionaryValue* old_settings,
    std::set<std::string>* changed_keys)
    : inner_(new Inner(settings, old_settings, changed_keys, std::string())) {}

ExtensionSettingsStorage::Result::Result(const std::string& error)
    : inner_(new Inner(NULL, NULL, new std::set<std::string>(), error)) {
  DCHECK(!error.empty());
}

ExtensionSettingsStorage::Result::~Result() {}

const DictionaryValue* ExtensionSettingsStorage::Result::GetSettings() const {
  DCHECK(!HasError());
  return inner_->settings_.get();
}

const std::set<std::string>*
ExtensionSettingsStorage::Result::GetChangedKeys() const {
  DCHECK(!HasError());
  return inner_->changed_keys_.get();
}

const Value* ExtensionSettingsStorage::Result::GetOldValue(
    const std::string& key) const {
  DCHECK(!HasError());
  if (!inner_->changed_keys_.get()) {
    return NULL;
  }
  Value* old_value = NULL;
  if (inner_->changed_keys_->count(key)) {
    inner_->old_settings_->GetWithoutPathExpansion(key, &old_value);
  } else if (inner_->settings_.get()) {
    inner_->settings_->GetWithoutPathExpansion(key, &old_value);
  }
  return old_value;
}

const Value* ExtensionSettingsStorage::Result::GetNewValue(
    const std::string& key) const {
  DCHECK(!HasError());
  if (!inner_->changed_keys_.get()) {
    return NULL;
  }
  Value* new_value = NULL;
  if (inner_->settings_.get()) {
    inner_->settings_->GetWithoutPathExpansion(key, &new_value);
  }
  return new_value;
}

bool ExtensionSettingsStorage::Result::HasError() const {
  return !inner_->error_.empty();
}

const std::string& ExtensionSettingsStorage::Result::GetError() const {
  DCHECK(HasError());
  return inner_->error_;
}

ExtensionSettingsStorage::Result::Inner::Inner(
    DictionaryValue* settings,
    DictionaryValue* old_settings,
    std::set<std::string>* changed_keys,
    const std::string& error)
    : settings_(settings),
      old_settings_(old_settings),
      changed_keys_(changed_keys),
      error_(error) {}

ExtensionSettingsStorage::Result::Inner::~Inner() {}

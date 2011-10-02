// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// Implementation of ExtensionSettingsStorage::Result

ExtensionSettingsStorage::Result::Result(
    DictionaryValue* settings, std::set<std::string>* changed_keys)
    : inner_(new Inner(settings, changed_keys, std::string())) {}

ExtensionSettingsStorage::Result::Result(const std::string& error)
    : inner_(new Inner(NULL, new std::set<std::string>(), error)) {
  DCHECK(!error.empty());
}

ExtensionSettingsStorage::Result::~Result() {}

DictionaryValue* ExtensionSettingsStorage::Result::GetSettings() const {
  DCHECK(!HasError());
  return inner_->settings_.get();
}

std::set<std::string>*
ExtensionSettingsStorage::Result::GetChangedKeys() const {
  DCHECK(!HasError());
  return inner_->changed_keys_.get();
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
    std::set<std::string>* changed_keys,
    const std::string& error)
    : settings_(settings), changed_keys_(changed_keys), error_(error) {}

ExtensionSettingsStorage::Result::Inner::~Inner() {}

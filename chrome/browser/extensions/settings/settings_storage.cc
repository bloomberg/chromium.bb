// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_storage.h"

#include "base/logging.h"

namespace extensions {

// Implementation of ReadResult.

SettingsStorage::ReadResult::ReadResult(DictionaryValue* settings)
    : inner_(new Inner(settings, "")) {
  DCHECK(settings);
}

SettingsStorage::ReadResult::ReadResult(const std::string& error)
    : inner_(new Inner(NULL, error)) {
  DCHECK(!error.empty());
}

SettingsStorage::ReadResult::~ReadResult() {}

bool SettingsStorage::ReadResult::HasError() const {
  return !inner_->error_.empty();
}

const DictionaryValue& SettingsStorage::ReadResult::settings() const {
  DCHECK(!HasError());
  return *inner_->settings_;
}

const std::string& SettingsStorage::ReadResult::error() const {
  DCHECK(HasError());
  return inner_->error_;
}

SettingsStorage::ReadResult::Inner::Inner(
    DictionaryValue* settings, const std::string& error)
    : settings_(settings), error_(error) {}

SettingsStorage::ReadResult::Inner::~Inner() {}

// Implementation of WriteResult.

SettingsStorage::WriteResult::WriteResult(
    SettingChangeList* changes) : inner_(new Inner(changes, "")) {
  DCHECK(changes);
}

SettingsStorage::WriteResult::WriteResult(const std::string& error)
    : inner_(new Inner(NULL, error)) {
  DCHECK(!error.empty());
}

SettingsStorage::WriteResult::~WriteResult() {}

bool SettingsStorage::WriteResult::HasError() const {
  return !inner_->error_.empty();
}

const SettingChangeList&
SettingsStorage::WriteResult::changes() const {
  DCHECK(!HasError());
  return *inner_->changes_;
}

const std::string& SettingsStorage::WriteResult::error() const {
  DCHECK(HasError());
  return inner_->error_;
}

SettingsStorage::WriteResult::Inner::Inner(
    SettingChangeList* changes, const std::string& error)
    : changes_(changes), error_(error) {}

SettingsStorage::WriteResult::Inner::~Inner() {}

}  // namespace extensions

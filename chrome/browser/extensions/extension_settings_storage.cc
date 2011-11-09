// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_storage.h"

#include "base/logging.h"

// Implementation of ReadResult.

ExtensionSettingsStorage::ReadResult::ReadResult(DictionaryValue* settings)
    : inner_(new Inner(settings, "")) {
  DCHECK(settings);
}

ExtensionSettingsStorage::ReadResult::ReadResult(const std::string& error)
    : inner_(new Inner(NULL, error)) {
  DCHECK(!error.empty());
}

ExtensionSettingsStorage::ReadResult::~ReadResult() {}

bool ExtensionSettingsStorage::ReadResult::HasError() const {
  return !inner_->error_.empty();
}

const DictionaryValue& ExtensionSettingsStorage::ReadResult::settings() const {
  DCHECK(!HasError());
  return *inner_->settings_;
}

const std::string& ExtensionSettingsStorage::ReadResult::error() const {
  DCHECK(HasError());
  return inner_->error_;
}

ExtensionSettingsStorage::ReadResult::Inner::Inner(
    DictionaryValue* settings, const std::string& error)
    : settings_(settings), error_(error) {}

ExtensionSettingsStorage::ReadResult::Inner::~Inner() {}

// Implementation of WriteResult.

ExtensionSettingsStorage::WriteResult::WriteResult(
    ExtensionSettingChangeList* changes) : inner_(new Inner(changes, "")) {
  DCHECK(changes);
}

ExtensionSettingsStorage::WriteResult::WriteResult(const std::string& error)
    : inner_(new Inner(NULL, error)) {
  DCHECK(!error.empty());
}

ExtensionSettingsStorage::WriteResult::~WriteResult() {}

bool ExtensionSettingsStorage::WriteResult::HasError() const {
  return !inner_->error_.empty();
}

const ExtensionSettingChangeList&
ExtensionSettingsStorage::WriteResult::changes() const {
  DCHECK(!HasError());
  return *inner_->changes_;
}

const std::string& ExtensionSettingsStorage::WriteResult::error() const {
  DCHECK(HasError());
  return inner_->error_;
}

ExtensionSettingsStorage::WriteResult::Inner::Inner(
    ExtensionSettingChangeList* changes, const std::string& error)
    : changes_(changes), error_(error) {}

ExtensionSettingsStorage::WriteResult::Inner::~Inner() {}

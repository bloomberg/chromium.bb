// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// Implementation of ExtensionSettingsStorage::Result

ExtensionSettingsStorage::Result::Result(DictionaryValue* settings)
    : inner_(new Inner(settings, std::string())) {}

ExtensionSettingsStorage::Result::Result(const std::string& error)
    : inner_(new Inner(NULL, error)) {
  DCHECK(!error.empty());
}

ExtensionSettingsStorage::Result::~Result() {}

DictionaryValue* ExtensionSettingsStorage::Result::GetSettings() const {
  DCHECK(!HasError());
  return inner_->settings_.get();
}

bool ExtensionSettingsStorage::Result::HasError() const {
  return !inner_->error_.empty();
}

const std::string& ExtensionSettingsStorage::Result::GetError() const {
  DCHECK(HasError());
  return inner_->error_;
}

ExtensionSettingsStorage::Result::Inner::Inner(
    DictionaryValue* settings, const std::string& error)
    : settings_(settings), error_(error) {}

ExtensionSettingsStorage::Result::Inner::~Inner() {}

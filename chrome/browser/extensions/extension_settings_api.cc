// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_settings_api.h"
#include "chrome/browser/profiles/profile.h"

namespace {
const char* kUnsupportedArgumentType = "Unsupported argument type";
}  // namespace

// StorageResultCallback

SettingsFunction::StorageResultCallback::StorageResultCallback(
    SettingsFunction* settings_function)
    : settings_function_(settings_function) {
}

SettingsFunction::StorageResultCallback::~StorageResultCallback() {
}

void SettingsFunction::StorageResultCallback::OnSuccess(
    DictionaryValue* settings) {
  settings_function_->result_.reset(settings);
  settings_function_->SendResponse(true);
}

void SettingsFunction::StorageResultCallback::OnFailure(
    const std::string& message) {
  settings_function_->error_ = message;
  settings_function_->SendResponse(false);
}

// SettingsFunction

bool SettingsFunction::RunImpl() {
  profile()->GetExtensionService()->extension_settings()->GetStorage(
      extension_id(),
      base::Bind(&SettingsFunction::RunWithStorage, this));
  return true;
}

void SettingsFunction::RunWithStorage(ExtensionSettingsStorage* storage) {
  // Mimic how RunImpl() is handled in extensions code.
  if (!RunWithStorageImpl(storage)) {
    SendResponse(false);
  }
}

// Concrete settings functions

bool GetSettingsFunction::RunWithStorageImpl(
    ExtensionSettingsStorage* storage) {
  Value *input;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));

  std::string as_string;
  ListValue* as_list;
  if (input->IsType(Value::TYPE_NULL)) {
    storage->Get(new StorageResultCallback(this));
  } else if (input->GetAsString(&as_string)) {
    storage->Get(as_string, new StorageResultCallback(this));
  } else if (input->GetAsList(&as_list)) {
    storage->Get(*as_list, new StorageResultCallback(this));
  } else {
    error_ = kUnsupportedArgumentType;
    return false;
  }

  return true;
}

bool SetSettingsFunction::RunWithStorageImpl(
    ExtensionSettingsStorage* storage) {
  DictionaryValue *input;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &input));
  storage->Set(*input, new StorageResultCallback(this));
  return true;
}

bool RemoveSettingsFunction::RunWithStorageImpl(
    ExtensionSettingsStorage* storage) {
  Value *input;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));

  std::string as_string;
  ListValue* as_list;
  if (input->GetAsString(&as_string)) {
    storage->Remove(as_string, new StorageResultCallback(this));
  } else if (input->GetAsList(&as_list)) {
    storage->Remove(*as_list, new StorageResultCallback(this));
  } else {
    error_ = kUnsupportedArgumentType;
    return false;
  }

  return true;
}

bool ClearSettingsFunction::RunWithStorageImpl(
    ExtensionSettingsStorage* storage) {
  storage->Clear(new StorageResultCallback(this));
  return true;
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_settings_api.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"

namespace {
const char* kUnsupportedArgumentType = "Unsupported argument type";
}  // namespace

// SettingsFunction

bool SettingsFunction::RunImpl() {
  profile()->GetExtensionService()->extension_settings_frontend()->
      RunWithBackend(
          base::Bind(&SettingsFunction::RunWithBackendOnFileThread, this));
  return true;
}

void SettingsFunction::RunWithBackendOnFileThread(
    ExtensionSettingsBackend* backend) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool success = RunWithStorage(backend->GetStorage(extension_id()));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SettingsFunction::SendResponse, this, success));
}

bool SettingsFunction::UseResult(
    const ExtensionSettingsStorage::Result& storage_result) {
  if (storage_result.HasError()) {
    error_ = storage_result.GetError();
    return false;
  }
  DictionaryValue* settings = storage_result.GetSettings();
  result_.reset(settings == NULL ? NULL : settings->DeepCopy());
  return true;
}

// Concrete settings functions

// Adds all StringValues from a ListValue to a vector of strings.
static void AddAllStringValues(
    const ListValue& from, std::vector<std::string>* to) {
  DCHECK(to->empty());
  std::string as_string;
  for (ListValue::const_iterator it = from.begin(); it != from.end(); ++it) {
    if ((*it)->GetAsString(&as_string)) {
      to->push_back(as_string);
    }
  }
}

bool GetSettingsFunction::RunWithStorage(
    ExtensionSettingsStorage* storage) {
  Value *input;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));
  std::string as_string;
  ListValue* as_list;
  if (input->IsType(Value::TYPE_NULL)) {
    return UseResult(storage->Get());
  } else if (input->GetAsString(&as_string)) {
    return UseResult(storage->Get(as_string));
  } else if (input->GetAsList(&as_list)) {
    std::vector<std::string> string_list;
    AddAllStringValues(*as_list, &string_list);
    return UseResult(storage->Get(string_list));
  }
  return UseResult(ExtensionSettingsStorage::Result(kUnsupportedArgumentType));
}

bool SetSettingsFunction::RunWithStorage(
    ExtensionSettingsStorage* storage) {
  DictionaryValue *input;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &input));
  return UseResult(storage->Set(*input));
}

bool RemoveSettingsFunction::RunWithStorage(
    ExtensionSettingsStorage* storage) {
  Value *input;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));
  std::string as_string;
  ListValue* as_list;
  if (input->GetAsString(&as_string)) {
    return UseResult(storage->Remove(as_string));
  } else if (input->GetAsList(&as_list)) {
    std::vector<std::string> string_list;
    AddAllStringValues(*as_list, &string_list);
    return UseResult(storage->Remove(string_list));
  }
  return UseResult(ExtensionSettingsStorage::Result(kUnsupportedArgumentType));
}

bool ClearSettingsFunction::RunWithStorage(
    ExtensionSettingsStorage* storage) {
  return UseResult(storage->Clear());
}

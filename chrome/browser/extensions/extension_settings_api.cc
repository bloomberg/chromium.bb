// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_settings_api.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

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
  bool success = RunWithStorage(backend, backend->GetStorage(extension_id()));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SettingsFunction::SendResponse, this, success));
}

bool SettingsFunction::UseResult(
    ExtensionSettingsBackend* backend,
    const ExtensionSettingsStorage::Result& storage_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (storage_result.HasError()) {
    error_ = storage_result.GetError();
    return false;
  }

  const DictionaryValue* settings = storage_result.GetSettings();
  if (settings) {
    result_.reset(settings->DeepCopy());
  }

  const std::set<std::string>* changed_keys = storage_result.GetChangedKeys();
  if (changed_keys && !changed_keys->empty()) {
    ExtensionSettingChanges::Builder changes;
    for (std::set<std::string>::const_iterator it = changed_keys->begin();
        it != changed_keys->end(); ++it) {
      const Value* old_value = storage_result.GetOldValue(*it);
      const Value* new_value = storage_result.GetNewValue(*it);
      changes.AppendChange(
          *it,
          old_value ? old_value->DeepCopy() : NULL,
          new_value ? new_value->DeepCopy() : NULL);
    }
    backend->TriggerOnSettingsChanged(
        profile(), extension_id(), changes.Build());
  }

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
    ExtensionSettingsBackend* backend,
    ExtensionSettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Value *input;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));
  std::string as_string;
  ListValue* as_list;
  if (input->IsType(Value::TYPE_NULL)) {
    return UseResult(backend, storage->Get());
  } else if (input->GetAsString(&as_string)) {
    return UseResult(backend, storage->Get(as_string));
  } else if (input->GetAsList(&as_list)) {
    std::vector<std::string> string_list;
    AddAllStringValues(*as_list, &string_list);
    return UseResult(backend, storage->Get(string_list));
  }
  return UseResult(
      backend, ExtensionSettingsStorage::Result(kUnsupportedArgumentType));
}

bool SetSettingsFunction::RunWithStorage(
    ExtensionSettingsBackend* backend,
    ExtensionSettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DictionaryValue *input;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &input));
  return UseResult(backend, storage->Set(*input));
}

bool RemoveSettingsFunction::RunWithStorage(
    ExtensionSettingsBackend* backend,
    ExtensionSettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Value *input;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));
  std::string as_string;
  ListValue* as_list;
  if (input->GetAsString(&as_string)) {
    return UseResult(backend, storage->Remove(as_string));
  } else if (input->GetAsList(&as_list)) {
    std::vector<std::string> string_list;
    AddAllStringValues(*as_list, &string_list);
    return UseResult(backend, storage->Remove(string_list));
  }
  return UseResult(
      backend, ExtensionSettingsStorage::Result(kUnsupportedArgumentType));
}

bool ClearSettingsFunction::RunWithStorage(
    ExtensionSettingsBackend* backend,
    ExtensionSettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return UseResult(backend, storage->Clear());
}

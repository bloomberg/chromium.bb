// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_settings_api.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {
const char* kUnsupportedArgumentType = "Unsupported argument type";
}  // namespace

// SettingsFunction

bool SettingsFunction::RunImpl() {
  ExtensionSettingsFrontend* frontend =
      profile()->GetExtensionService()->extension_settings_frontend();
  frontend->RunWithStorage(
      extension_id(),
      base::Bind(
          &SettingsFunction::RunWithStorageOnFileThread,
          this,
          frontend->GetObservers()));
  return true;
}

void SettingsFunction::RunWithStorageOnFileThread(
    scoped_refptr<ExtensionSettingsObserverList> observers,
    ExtensionSettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool success = false;
  if (storage) {
    success = RunWithStorage(observers.get(), storage);
  }
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SettingsFunction::SendResponse, this, success));
}

bool SettingsFunction::UseResult(
    scoped_refptr<ExtensionSettingsObserverList> observers,
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
    observers->Notify(
        &ExtensionSettingsObserver::OnSettingsChanged,
        profile(),
        extension_id(),
        changes.Build());
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
    scoped_refptr<ExtensionSettingsObserverList> observers,
    ExtensionSettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Value *input;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));
  std::string as_string;
  ListValue* as_list;
  if (input->IsType(Value::TYPE_NULL)) {
    return UseResult(observers, storage->Get());
  } else if (input->GetAsString(&as_string)) {
    return UseResult(observers, storage->Get(as_string));
  } else if (input->GetAsList(&as_list)) {
    std::vector<std::string> string_list;
    AddAllStringValues(*as_list, &string_list);
    return UseResult(observers, storage->Get(string_list));
  }
  return UseResult(
      observers, ExtensionSettingsStorage::Result(kUnsupportedArgumentType));
}

bool SetSettingsFunction::RunWithStorage(
    scoped_refptr<ExtensionSettingsObserverList> observers,
    ExtensionSettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DictionaryValue *input;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &input));
  return UseResult(observers, storage->Set(*input));
}

bool RemoveSettingsFunction::RunWithStorage(
    scoped_refptr<ExtensionSettingsObserverList> observers,
    ExtensionSettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Value *input;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));
  std::string as_string;
  ListValue* as_list;
  if (input->GetAsString(&as_string)) {
    return UseResult(observers, storage->Remove(as_string));
  } else if (input->GetAsList(&as_list)) {
    std::vector<std::string> string_list;
    AddAllStringValues(*as_list, &string_list);
    return UseResult(observers, storage->Remove(string_list));
  }
  return UseResult(
      observers, ExtensionSettingsStorage::Result(kUnsupportedArgumentType));
}

bool ClearSettingsFunction::RunWithStorage(
    scoped_refptr<ExtensionSettingsObserverList> observers,
    ExtensionSettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return UseResult(observers, storage->Clear());
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

using content::BrowserThread;

namespace {
const char* kUnsupportedArgumentType = "Unsupported argument type";
}  // namespace

// SettingsFunction

bool SettingsFunction::RunImpl() {
  SettingsFrontend* frontend =
      profile()->GetExtensionService()->settings_frontend();
  frontend->RunWithStorage(
      extension_id(),
      base::Bind(
          &SettingsFunction::RunWithStorageOnFileThread,
          this,
          frontend->GetObservers()));
  return true;
}

void SettingsFunction::RunWithStorageOnFileThread(
    scoped_refptr<SettingsObserverList> observers,
    SettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool success = RunWithStorage(observers.get(), storage);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SettingsFunction::SendResponse, this, success));
}

bool SettingsFunction::UseReadResult(
    const SettingsStorage::ReadResult& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (result.HasError()) {
    error_ = result.error();
    return false;
  }

  result_.reset(result.settings().DeepCopy());
  return true;
}

bool SettingsFunction::UseWriteResult(
    scoped_refptr<SettingsObserverList> observers,
    const SettingsStorage::WriteResult& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (result.HasError()) {
    error_ = result.error();
    return false;
  }

  if (!result.changes().empty()) {
    observers->Notify(
        &SettingsObserver::OnSettingsChanged,
        extension_id(),
        SettingChange::GetEventJson(result.changes()));
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

// Gets the keys of a DictionaryValue.
static std::vector<std::string> GetKeys(const DictionaryValue& dict) {
  std::vector<std::string> keys;
  for (DictionaryValue::key_iterator it = dict.begin_keys();
      it != dict.end_keys(); ++it) {
    keys.push_back(*it);
  }
  return keys;
}

bool GetSettingsFunction::RunWithStorage(
    scoped_refptr<SettingsObserverList> observers,
    SettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Value *input;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));

  switch (input->GetType()) {
    case Value::TYPE_NULL:
      return UseReadResult(storage->Get());

    case Value::TYPE_STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      return UseReadResult(storage->Get(as_string));
    }

    case Value::TYPE_LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<ListValue*>(input), &as_string_list);
      return UseReadResult(storage->Get(as_string_list));
    }

    case Value::TYPE_DICTIONARY: {
      DictionaryValue* as_dict = static_cast<DictionaryValue*>(input);
      SettingsStorage::ReadResult result =
          storage->Get(GetKeys(*as_dict));
      if (result.HasError()) {
        return UseReadResult(result);
      }

      DictionaryValue* with_default_values = as_dict->DeepCopy();
      with_default_values->MergeDictionary(&result.settings());
      return UseReadResult(
          SettingsStorage::ReadResult(with_default_values));
    }

    default:
      return UseReadResult(
          SettingsStorage::ReadResult(kUnsupportedArgumentType));
  }
}

bool SetSettingsFunction::RunWithStorage(
    scoped_refptr<SettingsObserverList> observers,
    SettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DictionaryValue *input;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &input));
  return UseWriteResult(
      observers, storage->Set(SettingsStorage::DEFAULTS, *input));
}

bool RemoveSettingsFunction::RunWithStorage(
    scoped_refptr<SettingsObserverList> observers,
    SettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Value *input;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));

  switch (input->GetType()) {
    case Value::TYPE_STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      return UseWriteResult(observers, storage->Remove(as_string));
    }

    case Value::TYPE_LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<ListValue*>(input), &as_string_list);
      return UseWriteResult(observers, storage->Remove(as_string_list));
    }

    default:
      return UseWriteResult(
          observers,
          SettingsStorage::WriteResult(kUnsupportedArgumentType));
  };
}

bool ClearSettingsFunction::RunWithStorage(
    scoped_refptr<SettingsObserverList> observers,
    SettingsStorage* storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return UseWriteResult(observers, storage->Clear());
}

}  // namespace extensions

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_api.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/storage.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

using content::BrowserThread;

namespace {
const char kUnsupportedArgumentType[] = "Unsupported argument type";
const char kInvalidNamespaceErrorMessage[] =
    "\"%s\" is not available in this instance of Chrome";
const char kManagedNamespaceDisabledErrorMessage[] =
    "\"managed\" is disabled. Use \"--%s\" to enable it.";
const char kStorageErrorMessage[] = "Storage error";
}  // namespace

// SettingsFunction

SettingsFunction::SettingsFunction()
    : settings_namespace_(settings_namespace::INVALID) {}

SettingsFunction::~SettingsFunction() {}

bool SettingsFunction::ShouldSkipQuotaLimiting() const {
  // Only apply quota if this is for sync storage.
  std::string settings_namespace_string;
  if (!args_->GetString(0, &settings_namespace_string)) {
    // This is an error but it will be caught in RunImpl(), there is no
    // mechanism to signify an error from this function.
    return false;
  }
  return settings_namespace_string != "sync";
}

bool SettingsFunction::RunImpl() {
  std::string settings_namespace_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &settings_namespace_string));
  args_->Remove(0, NULL);
  settings_namespace_ =
      settings_namespace::FromString(settings_namespace_string);
  EXTENSION_FUNCTION_VALIDATE(
      settings_namespace_ != settings_namespace::INVALID);

  SettingsFrontend* frontend =
      profile()->GetExtensionService()->settings_frontend();
  if (!frontend->IsStorageEnabled(settings_namespace_)) {
    error_ = base::StringPrintf(kInvalidNamespaceErrorMessage,
                                settings_namespace_string.c_str());
    return false;
  }

  observers_ = frontend->GetObservers();
  frontend->RunWithStorage(
      extension_id(),
      settings_namespace_,
      base::Bind(&SettingsFunction::AsyncRunWithStorage, this));
  return true;
}

void SettingsFunction::AsyncRunWithStorage(ValueStore* storage) {
  bool success = RunWithStorage(storage);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SettingsFunction::SendResponse, this, success));
}

bool SettingsFunction::UseReadResult(ValueStore::ReadResult result) {
  if (result->HasError()) {
    error_ = result->error();
    return false;
  }

  SetResult(result->settings().release());
  return true;
}

bool SettingsFunction::UseWriteResult(ValueStore::WriteResult result) {
  if (result->HasError()) {
    error_ = result->error();
    return false;
  }

  if (!result->changes().empty()) {
    observers_->Notify(
        &SettingsObserver::OnSettingsChanged,
        extension_id(),
        settings_namespace_,
        ValueStoreChange::ToJson(result->changes()));
  }

  return true;
}

// Concrete settings functions

namespace {

// Adds all StringValues from a ListValue to a vector of strings.
void AddAllStringValues(const ListValue& from, std::vector<std::string>* to) {
  DCHECK(to->empty());
  std::string as_string;
  for (ListValue::const_iterator it = from.begin(); it != from.end(); ++it) {
    if ((*it)->GetAsString(&as_string)) {
      to->push_back(as_string);
    }
  }
}

// Gets the keys of a DictionaryValue.
std::vector<std::string> GetKeys(const DictionaryValue& dict) {
  std::vector<std::string> keys;
  for (DictionaryValue::key_iterator it = dict.begin_keys();
      it != dict.end_keys(); ++it) {
    keys.push_back(*it);
  }
  return keys;
}

// Creates quota heuristics for settings modification.
void GetModificationQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) {
  QuotaLimitHeuristic::Config longLimitConfig = {
    // See storage.json for current value.
    api::storage::sync::MAX_WRITE_OPERATIONS_PER_HOUR,
    base::TimeDelta::FromHours(1)
  };
  heuristics->push_back(
      new ExtensionsQuotaService::TimedLimit(
          longLimitConfig,
          new QuotaLimitHeuristic::SingletonBucketMapper(),
          "MAX_WRITE_OPERATIONS_PER_HOUR"));

  // A max of 10 operations per minute, sustained over 10 minutes.
  QuotaLimitHeuristic::Config shortLimitConfig = {
    // See storage.json for current value.
    api::storage::sync::MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE,
    base::TimeDelta::FromMinutes(1)
  };
  heuristics->push_back(
      new ExtensionsQuotaService::SustainedLimit(
          base::TimeDelta::FromMinutes(10),
          shortLimitConfig,
          new QuotaLimitHeuristic::SingletonBucketMapper(),
          "MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE"));
};

}  // namespace

bool GetSettingsFunction::RunWithStorage(ValueStore* storage) {
  Value* input = NULL;
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
      ValueStore::ReadResult result = storage->Get(GetKeys(*as_dict));
      if (result->HasError()) {
        return UseReadResult(result.Pass());
      }

      DictionaryValue* with_default_values = as_dict->DeepCopy();
      with_default_values->MergeDictionary(result->settings().get());
      return UseReadResult(
          ValueStore::MakeReadResult(with_default_values));
    }

    default:
      return UseReadResult(
          ValueStore::MakeReadResult(kUnsupportedArgumentType));
  }
}

bool GetBytesInUseSettingsFunction::RunWithStorage(ValueStore* storage) {
  Value* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));

  size_t bytes_in_use = 0;

  switch (input->GetType()) {
    case Value::TYPE_NULL:
      bytes_in_use = storage->GetBytesInUse();
      break;

    case Value::TYPE_STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      bytes_in_use = storage->GetBytesInUse(as_string);
      break;
    }

    case Value::TYPE_LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<ListValue*>(input), &as_string_list);
      bytes_in_use = storage->GetBytesInUse(as_string_list);
      break;
    }

    default:
      error_ = kUnsupportedArgumentType;
      return false;
  }

  SetResult(Value::CreateIntegerValue(bytes_in_use));
  return true;
}

bool SetSettingsFunction::RunWithStorage(ValueStore* storage) {
  DictionaryValue* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &input));
  return UseWriteResult(storage->Set(ValueStore::DEFAULTS, *input));
}

void SetSettingsFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

bool RemoveSettingsFunction::RunWithStorage(ValueStore* storage) {
  Value* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));

  switch (input->GetType()) {
    case Value::TYPE_STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      return UseWriteResult(storage->Remove(as_string));
    }

    case Value::TYPE_LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<ListValue*>(input), &as_string_list);
      return UseWriteResult(storage->Remove(as_string_list));
    }

    default:
      return UseWriteResult(
          ValueStore::MakeWriteResult(kUnsupportedArgumentType));
  };
}

void RemoveSettingsFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

bool ClearSettingsFunction::RunWithStorage(ValueStore* storage) {
  return UseWriteResult(storage->Clear());
}

void ClearSettingsFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

}  // namespace extensions

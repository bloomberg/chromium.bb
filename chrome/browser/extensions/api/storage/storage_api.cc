// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/storage_api.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/storage/settings_frontend.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/storage.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

using content::BrowserThread;

// SettingsFunction

SettingsFunction::SettingsFunction()
    : settings_namespace_(settings_namespace::INVALID) {}

SettingsFunction::~SettingsFunction() {}

bool SettingsFunction::ShouldSkipQuotaLimiting() const {
  // Only apply quota if this is for sync storage.
  std::string settings_namespace_string;
  if (!args_->GetString(0, &settings_namespace_string)) {
    // This should be EXTENSION_FUNCTION_VALIDATE(false) but there is no way
    // to signify that from this function. It will be caught in RunImpl().
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
      GetProfile()->GetExtensionService()->settings_frontend();
  if (!frontend->IsStorageEnabled(settings_namespace_)) {
    error_ = base::StringPrintf(
        "\"%s\" is not available in this instance of Chrome",
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

bool SettingsFunction::UseReadResult(ValueStore::ReadResult read_result) {
  if (read_result->HasError()) {
    error_ = read_result->error().message;
    return false;
  }

  base::DictionaryValue* result = new base::DictionaryValue();
  result->Swap(&read_result->settings());
  SetResult(result);
  return true;
}

bool SettingsFunction::UseWriteResult(ValueStore::WriteResult result) {
  if (result->HasError()) {
    error_ = result->error().message;
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
void AddAllStringValues(const base::ListValue& from,
                        std::vector<std::string>* to) {
  DCHECK(to->empty());
  std::string as_string;
  for (base::ListValue::const_iterator it = from.begin();
       it != from.end(); ++it) {
    if ((*it)->GetAsString(&as_string)) {
      to->push_back(as_string);
    }
  }
}

// Gets the keys of a DictionaryValue.
std::vector<std::string> GetKeys(const base::DictionaryValue& dict) {
  std::vector<std::string> keys;
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    keys.push_back(it.key());
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

bool StorageStorageAreaGetFunction::RunWithStorage(ValueStore* storage) {
  base::Value* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));

  switch (input->GetType()) {
    case base::Value::TYPE_NULL:
      return UseReadResult(storage->Get());

    case base::Value::TYPE_STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      return UseReadResult(storage->Get(as_string));
    }

    case base::Value::TYPE_LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<base::ListValue*>(input),
                         &as_string_list);
      return UseReadResult(storage->Get(as_string_list));
    }

    case base::Value::TYPE_DICTIONARY: {
      base::DictionaryValue* as_dict = static_cast<base::DictionaryValue*>(input);
      ValueStore::ReadResult result = storage->Get(GetKeys(*as_dict));
      if (result->HasError()) {
        return UseReadResult(result.Pass());
      }

      base::DictionaryValue* with_default_values = as_dict->DeepCopy();
      with_default_values->MergeDictionary(&result->settings());
      return UseReadResult(
          ValueStore::MakeReadResult(make_scoped_ptr(with_default_values)));
    }

    default:
      EXTENSION_FUNCTION_VALIDATE(false);
      return false;
  }
}

bool StorageStorageAreaGetBytesInUseFunction::RunWithStorage(
    ValueStore* storage) {
  base::Value* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));

  size_t bytes_in_use = 0;

  switch (input->GetType()) {
    case base::Value::TYPE_NULL:
      bytes_in_use = storage->GetBytesInUse();
      break;

    case base::Value::TYPE_STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      bytes_in_use = storage->GetBytesInUse(as_string);
      break;
    }

    case base::Value::TYPE_LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<base::ListValue*>(input),
                         &as_string_list);
      bytes_in_use = storage->GetBytesInUse(as_string_list);
      break;
    }

    default:
      EXTENSION_FUNCTION_VALIDATE(false);
      return false;
  }

  SetResult(new base::FundamentalValue(static_cast<int>(bytes_in_use)));
  return true;
}

bool StorageStorageAreaSetFunction::RunWithStorage(ValueStore* storage) {
  base::DictionaryValue* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &input));
  return UseWriteResult(storage->Set(ValueStore::DEFAULTS, *input));
}

void StorageStorageAreaSetFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

bool StorageStorageAreaRemoveFunction::RunWithStorage(ValueStore* storage) {
  base::Value* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &input));

  switch (input->GetType()) {
    case base::Value::TYPE_STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      return UseWriteResult(storage->Remove(as_string));
    }

    case base::Value::TYPE_LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<base::ListValue*>(input),
                         &as_string_list);
      return UseWriteResult(storage->Remove(as_string_list));
    }

    default:
      EXTENSION_FUNCTION_VALIDATE(false);
      return false;
  };
}

void StorageStorageAreaRemoveFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

bool StorageStorageAreaClearFunction::RunWithStorage(ValueStore* storage) {
  return UseWriteResult(storage->Clear());
}

void StorageStorageAreaClearFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

}  // namespace extensions

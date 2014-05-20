// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_api.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/quota_service.h"
#include "extensions/common/api/storage.h"

namespace extensions {

using content::BrowserThread;

// SettingsFunction

SettingsFunction::SettingsFunction()
    : settings_namespace_(settings_namespace::INVALID),
      tried_restoring_storage_(false) {}

SettingsFunction::~SettingsFunction() {}

bool SettingsFunction::ShouldSkipQuotaLimiting() const {
  // Only apply quota if this is for sync storage.
  std::string settings_namespace_string;
  if (!args_->GetString(0, &settings_namespace_string)) {
    // This should be EXTENSION_FUNCTION_VALIDATE(false) but there is no way
    // to signify that from this function. It will be caught in Run().
    return false;
  }
  return settings_namespace_string != "sync";
}

ExtensionFunction::ResponseAction SettingsFunction::Run() {
  std::string settings_namespace_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &settings_namespace_string));
  args_->Remove(0, NULL);
  settings_namespace_ =
      settings_namespace::FromString(settings_namespace_string);
  EXTENSION_FUNCTION_VALIDATE(settings_namespace_ !=
                              settings_namespace::INVALID);

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  if (!frontend->IsStorageEnabled(settings_namespace_)) {
    return RespondNow(Error(
        base::StringPrintf("\"%s\" is not available in this instance of Chrome",
                           settings_namespace_string.c_str())));
  }

  observers_ = frontend->GetObservers();
  frontend->RunWithStorage(
      GetExtension(),
      settings_namespace_,
      base::Bind(&SettingsFunction::AsyncRunWithStorage, this));
  return RespondLater();
}

void SettingsFunction::AsyncRunWithStorage(ValueStore* storage) {
  ResponseValue response = RunWithStorage(storage);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SettingsFunction::Respond, this, base::Passed(&response)));
}

ExtensionFunction::ResponseValue SettingsFunction::UseReadResult(
    ValueStore::ReadResult result,
    ValueStore* storage) {
  if (result->HasError())
    return HandleError(result->error(), storage);

  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->Swap(&result->settings());
  return OneArgument(dict);
}

ExtensionFunction::ResponseValue SettingsFunction::UseWriteResult(
    ValueStore::WriteResult result,
    ValueStore* storage) {
  if (result->HasError())
    return HandleError(result->error(), storage);

  if (!result->changes().empty()) {
    observers_->Notify(
        &SettingsObserver::OnSettingsChanged,
        extension_id(),
        settings_namespace_,
        ValueStoreChange::ToJson(result->changes()));
  }

  return NoArguments();
}

ExtensionFunction::ResponseValue SettingsFunction::HandleError(
    const ValueStore::Error& error,
    ValueStore* storage) {
  // If the method failed due to corruption, and we haven't tried to fix it, we
  // can try to restore the storage and re-run it. Otherwise, the method has
  // failed.
  if (error.code == ValueStore::CORRUPTION && !tried_restoring_storage_) {
    tried_restoring_storage_ = true;

    // If the corruption is on a particular key, try to restore that key and
    // re-run.
    if (error.key.get() && storage->RestoreKey(*error.key))
      return RunWithStorage(storage);

    // If the full database is corrupted, try to restore the whole thing and
    // re-run.
    if (storage->Restore())
      return RunWithStorage(storage);
  }

  return Error(error.message);
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
    core_api::storage::sync::MAX_WRITE_OPERATIONS_PER_HOUR,
    base::TimeDelta::FromHours(1)
  };
  heuristics->push_back(new QuotaService::TimedLimit(
      longLimitConfig,
      new QuotaLimitHeuristic::SingletonBucketMapper(),
      "MAX_WRITE_OPERATIONS_PER_HOUR"));

  // A max of 10 operations per minute, sustained over 10 minutes.
  QuotaLimitHeuristic::Config shortLimitConfig = {
    // See storage.json for current value.
    core_api::storage::sync::MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE,
    base::TimeDelta::FromMinutes(1)
  };
  heuristics->push_back(new QuotaService::SustainedLimit(
      base::TimeDelta::FromMinutes(10),
      shortLimitConfig,
      new QuotaLimitHeuristic::SingletonBucketMapper(),
      "MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE"));
};

}  // namespace

ExtensionFunction::ResponseValue StorageStorageAreaGetFunction::RunWithStorage(
    ValueStore* storage) {
  base::Value* input = NULL;
  if (!args_->Get(0, &input))
    return BadMessage();

  switch (input->GetType()) {
    case base::Value::TYPE_NULL:
      return UseReadResult(storage->Get(), storage);

    case base::Value::TYPE_STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      return UseReadResult(storage->Get(as_string), storage);
    }

    case base::Value::TYPE_LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<base::ListValue*>(input),
                         &as_string_list);
      return UseReadResult(storage->Get(as_string_list), storage);
    }

    case base::Value::TYPE_DICTIONARY: {
      base::DictionaryValue* as_dict =
          static_cast<base::DictionaryValue*>(input);
      ValueStore::ReadResult result = storage->Get(GetKeys(*as_dict));
      if (result->HasError()) {
        return UseReadResult(result.Pass(), storage);
      }

      base::DictionaryValue* with_default_values = as_dict->DeepCopy();
      with_default_values->MergeDictionary(&result->settings());
      return UseReadResult(
          ValueStore::MakeReadResult(make_scoped_ptr(with_default_values)),
          storage);
    }

    default:
      return BadMessage();
  }
}

ExtensionFunction::ResponseValue
StorageStorageAreaGetBytesInUseFunction::RunWithStorage(ValueStore* storage) {
  base::Value* input = NULL;
  if (!args_->Get(0, &input))
    return BadMessage();

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
      return BadMessage();
  }

  return OneArgument(
      new base::FundamentalValue(static_cast<int>(bytes_in_use)));
}

ExtensionFunction::ResponseValue StorageStorageAreaSetFunction::RunWithStorage(
    ValueStore* storage) {
  base::DictionaryValue* input = NULL;
  if (!args_->GetDictionary(0, &input))
    return BadMessage();
  return UseWriteResult(storage->Set(ValueStore::DEFAULTS, *input), storage);
}

void StorageStorageAreaSetFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseValue
StorageStorageAreaRemoveFunction::RunWithStorage(ValueStore* storage) {
  base::Value* input = NULL;
  if (!args_->Get(0, &input))
    return BadMessage();

  switch (input->GetType()) {
    case base::Value::TYPE_STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      return UseWriteResult(storage->Remove(as_string), storage);
    }

    case base::Value::TYPE_LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<base::ListValue*>(input),
                         &as_string_list);
      return UseWriteResult(storage->Remove(as_string_list), storage);
    }

    default:
      return BadMessage();
  };
}

void StorageStorageAreaRemoveFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseValue
StorageStorageAreaClearFunction::RunWithStorage(ValueStore* storage) {
  return UseWriteResult(storage->Clear(), storage);
}

void StorageStorageAreaClearFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

}  // namespace extensions

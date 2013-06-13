// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/history_data_store.h"

#include "base/callback.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace app_list {

namespace {

const char kKeyVersion[] = "version";
const char kCurrentVersion[] = "1";

const char kKeyAssociations[] = "associations";
const char kKeyPrimary[] = "p";
const char kKeySecondary[] = "s";
const char kKeyUpdateTime[] = "t";

// Extracts strings from ListValue.
void GetSecondary(const base::ListValue* list,
                  HistoryData::SecondaryDeque* secondary) {
  HistoryData::SecondaryDeque results;
  for (base::ListValue::const_iterator it = list->begin();
       it != list->end(); ++it) {
    std::string str;
    if (!(*it)->GetAsString(&str))
      return;

    results.push_back(str);
  }

  secondary->swap(results);
}

// V1 format json dictionary:
//  {
//    "version": "1",
//    "associations": {
//      "user typed query": {
//        "p" : "result id of primary association",
//        "s" : [
//                "result id of 1st (oldest) secondary association",
//                ...
//                "result id of the newest secondary association"
//              ],
//        "t" : "last_update_timestamp"
//      },
//      ...
//    }
//  }
scoped_ptr<HistoryData::Associations> Parse(const base::DictionaryValue& dict) {
  std::string version;
  if (!dict.GetStringWithoutPathExpansion(kKeyVersion, &version) ||
      version != kCurrentVersion) {
    return scoped_ptr<HistoryData::Associations>();
  }

  const base::DictionaryValue* assoc_dict = NULL;
  if (!dict.GetDictionaryWithoutPathExpansion(kKeyAssociations, &assoc_dict) ||
      !assoc_dict) {
    return scoped_ptr<HistoryData::Associations>();
  }

  scoped_ptr<HistoryData::Associations> data(new HistoryData::Associations);
  for (base::DictionaryValue::Iterator it(*assoc_dict);
       !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* entry_dict = NULL;
    if (!it.value().GetAsDictionary(&entry_dict))
      continue;

    std::string primary;
    std::string update_time_string;
    if (!entry_dict->GetStringWithoutPathExpansion(kKeyPrimary, &primary) ||
        !entry_dict->GetStringWithoutPathExpansion(kKeyUpdateTime,
                                                   &update_time_string)) {
      continue;
    }

    const base::ListValue* secondary_list = NULL;
    HistoryData::SecondaryDeque secondary;
    if (entry_dict->GetListWithoutPathExpansion(kKeySecondary, &secondary_list))
      GetSecondary(secondary_list, &secondary);

    const std::string& query = it.key();
    HistoryData::Data& association_data = (*data.get())[query];
    association_data.primary = primary;
    association_data.secondary.swap(secondary);

    int64 update_time_val;
    base::StringToInt64(update_time_string, &update_time_val);
    association_data.update_time =
        base::Time::FromInternalValue(update_time_val);
  }

  return data.Pass();
}

// An empty callback used to ensure file tasks are cleared.
void EmptyCallback() {}

}  // namespace

HistoryDataStore::HistoryDataStore(const base::FilePath& data_file)
    : data_file_(data_file) {
  std::string token("app-launcher-history-data-store");
  token.append(data_file.AsUTF8Unsafe());

  // Uses a SKIP_ON_SHUTDOWN file task runner because losing a couple
  // associations is better than blocking shutdown.
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  file_task_runner_ = pool->GetSequencedTaskRunnerWithShutdownBehavior(
      pool->GetNamedSequenceToken(token),
      base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  writer_.reset(
      new base::ImportantFileWriter(data_file, file_task_runner_.get()));

  cached_json_.reset(new base::DictionaryValue);
  cached_json_->SetString(kKeyVersion, kCurrentVersion);
  cached_json_->Set(kKeyAssociations, new base::DictionaryValue);
}

HistoryDataStore::~HistoryDataStore() {
  Flush(OnFlushedCallback());
}

void HistoryDataStore::Flush(const OnFlushedCallback& on_flushed) {
  if (writer_->HasPendingWrite())
    writer_->DoScheduledWrite();

  if (on_flushed.is_null())
    return;

  file_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&EmptyCallback), on_flushed);
}

void HistoryDataStore::Load(
    const HistoryDataStore::OnLoadedCallback& on_loaded) {
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&HistoryDataStore::LoadOnBlockingPool, this),
      on_loaded);
}

void HistoryDataStore::SetPrimary(const std::string& query,
                                  const std::string& result) {
  base::DictionaryValue* entry_dict = GetEntryDict(query);
  entry_dict->SetWithoutPathExpansion(kKeyPrimary,
                                      new base::StringValue(result));
  writer_->ScheduleWrite(this);
}

void HistoryDataStore::SetSecondary(
    const std::string& query,
    const HistoryData::SecondaryDeque& results) {
  scoped_ptr<base::ListValue> results_list(new base::ListValue);
  for (size_t i = 0; i< results.size(); ++i)
    results_list->AppendString(results[i]);

  base::DictionaryValue* entry_dict = GetEntryDict(query);
  entry_dict->SetWithoutPathExpansion(kKeySecondary, results_list.release());
  writer_->ScheduleWrite(this);
}

void HistoryDataStore::SetUpdateTime(const std::string& query,
                                     const base::Time& update_time) {
  base::DictionaryValue* entry_dict = GetEntryDict(query);
  entry_dict->SetWithoutPathExpansion(kKeyUpdateTime,
                                      new base::StringValue(base::Int64ToString(
                                          update_time.ToInternalValue())));
  writer_->ScheduleWrite(this);
}

void HistoryDataStore::Delete(const std::string& query) {
  base::DictionaryValue* assoc_dict = GetAssociationDict();
  assoc_dict->RemoveWithoutPathExpansion(query, NULL);
  writer_->ScheduleWrite(this);
}

base::DictionaryValue* HistoryDataStore::GetAssociationDict() {
  base::DictionaryValue* assoc_dict = NULL;
  CHECK(cached_json_->GetDictionary(kKeyAssociations, &assoc_dict) &&
        assoc_dict);

  return assoc_dict;
}

base::DictionaryValue* HistoryDataStore::GetEntryDict(
    const std::string& query) {
  base::DictionaryValue* assoc_dict = GetAssociationDict();

  base::DictionaryValue* entry_dict = NULL;
  if (!assoc_dict->GetDictionaryWithoutPathExpansion(query, &entry_dict)) {
    // Creates one if none exists. Ownership is taken in the set call after.
    entry_dict = new base::DictionaryValue;
    assoc_dict->SetWithoutPathExpansion(query, entry_dict);
  }

  return entry_dict;
}

scoped_ptr<HistoryData::Associations> HistoryDataStore::LoadOnBlockingPool() {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  int error_code = JSONFileValueSerializer::JSON_NO_ERROR;
  std::string error_message;
  JSONFileValueSerializer serializer(data_file_);
  base::Value* value = serializer.Deserialize(&error_code, &error_message);
  base::DictionaryValue* dict_value = NULL;
  if (error_code != JSONFileValueSerializer::JSON_NO_ERROR ||
      !value ||
      !value->GetAsDictionary(&dict_value) ||
      !dict_value) {
    return scoped_ptr<HistoryData::Associations>();
  }

  cached_json_.reset(dict_value);
  return Parse(*dict_value).Pass();
}

bool HistoryDataStore::SerializeData(std::string* data) {
  JSONStringValueSerializer serializer(data);
  serializer.set_pretty_print(true);
  return serializer.Serialize(*cached_json_.get());
}

}  // namespace app_list

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/dictionary_data_store.h"

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

DictionaryDataStore::DictionaryDataStore(const base::FilePath& data_file)
    : data_file_(data_file) {
  std::string token("app-launcher-data-store");
  token.append(data_file.AsUTF8Unsafe());

  // Uses a SKIP_ON_SHUTDOWN file task runner because losing a couple
  // associations is better than blocking shutdown.
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  file_task_runner_ = pool->GetSequencedTaskRunnerWithShutdownBehavior(
      pool->GetNamedSequenceToken(token),
      base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  writer_.reset(
      new base::ImportantFileWriter(data_file, file_task_runner_.get()));

  cached_dict_.reset(new base::DictionaryValue);
}

DictionaryDataStore::~DictionaryDataStore() {
  Flush(OnFlushedCallback());
}

void DictionaryDataStore::Flush(const OnFlushedCallback& on_flushed) {
  if (writer_->HasPendingWrite())
    writer_->DoScheduledWrite();

  if (on_flushed.is_null())
    return;

  file_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&base::DoNothing), on_flushed);
}

void DictionaryDataStore::Load(
    const DictionaryDataStore::OnLoadedCallback& on_loaded) {
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(),
      FROM_HERE,
      base::Bind(&DictionaryDataStore::LoadOnBlockingPool, this),
      on_loaded);
}

void DictionaryDataStore::ScheduleWrite() {
  writer_->ScheduleWrite(this);
}

scoped_ptr<base::DictionaryValue> DictionaryDataStore::LoadOnBlockingPool() {
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
    return scoped_ptr<base::DictionaryValue>();
  }

  base::DictionaryValue* return_dict = dict_value->DeepCopy();
  cached_dict_.reset(dict_value);
  return make_scoped_ptr(return_dict).Pass();
}

bool DictionaryDataStore::SerializeData(std::string* data) {
  JSONStringValueSerializer serializer(data);
  serializer.set_pretty_print(true);
  return serializer.Serialize(*cached_dict_.get());
}

}  // namespace app_list

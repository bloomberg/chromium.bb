// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/task_runner_util.h"
#include "base/values.h"

namespace ntp_snippets {

bool ReadFileToString(const base::FilePath& path, std::string* data) {
  DCHECK(data);
  bool success = base::ReadFileToString(path, data);
  DLOG_IF(ERROR, !success) << "Error reading file " << path.LossyDisplayName();
  return success;
}

NTPSnippetsService::NTPSnippetsService(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    const std::string& application_language_code,
    scoped_ptr<NTPSnippetsFetcher> snippets_fetcher)
    : loaded_(false),
      file_task_runner_(file_task_runner),
      application_language_code_(application_language_code),
      snippets_fetcher_(std::move(snippets_fetcher)),
      weak_ptr_factory_(this) {
  snippets_fetcher_callback_ = snippets_fetcher_->AddCallback(
      base::Bind(&NTPSnippetsService::OnSnippetsDownloaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

NTPSnippetsService::~NTPSnippetsService() {}

void NTPSnippetsService::Shutdown() {
  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceShutdown(this));
  loaded_ = false;
}

void NTPSnippetsService::FetchSnippets(bool overwrite) {
  snippets_fetcher_->FetchSnippets(overwrite);
}

void NTPSnippetsService::AddObserver(NTPSnippetsServiceObserver* observer) {
  observers_.AddObserver(observer);
  if (loaded_)
    observer->NTPSnippetsServiceLoaded(this);
}

void NTPSnippetsService::RemoveObserver(NTPSnippetsServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NTPSnippetsService::OnFileReadDone(const std::string* json, bool success) {
  if (!success)
    return;

  DCHECK(json);
  LoadFromJSONString(*json);
}

bool NTPSnippetsService::LoadFromJSONString(const std::string& str) {
  JSONStringValueDeserializer deserializer(str);
  int error_code;
  std::string error_message;

  scoped_ptr<base::Value> deserialized =
      deserializer.Deserialize(&error_code, &error_message);
  if (!deserialized)
    return false;

  const base::DictionaryValue* top_dict = NULL;
  if (!deserialized->GetAsDictionary(&top_dict))
    return false;

  const base::ListValue* list = NULL;
  if (!top_dict->GetList("recos", &list))
    return false;

  for (base::Value* const value : *list) {
    const base::DictionaryValue* dict = NULL;
    if (!value->GetAsDictionary(&dict))
      return false;

    const base::DictionaryValue* content = NULL;
    if (!dict->GetDictionary("contentInfo", &content))
      return false;
    scoped_ptr<NTPSnippet> snippet =
        NTPSnippet::NTPSnippetFromDictionary(*content);
    if (!snippet)
      return false;
    snippets_.push_back(std::move(snippet));
  }
  loaded_ = true;

  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceLoaded(this));
  return true;
}

void NTPSnippetsService::OnSnippetsDownloaded(
    const base::FilePath& download_path) {
  std::string* downloaded_data = new std::string;
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::Bind(&ReadFileToString, download_path, downloaded_data),
      base::Bind(&NTPSnippetsService::OnFileReadDone,
                 weak_ptr_factory_.GetWeakPtr(), base::Owned(downloaded_data)));
}

}  // namespace ntp_snippets

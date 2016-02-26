// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/task_runner_util.h"
#include "base/values.h"

namespace {

// TODO(crbug.com/587857): This is an extremely small value, for development.
// Replace it by something sensible and add a command line param to control it.
const int kDefaultFetchingIntervalSeconds = 60;

bool ReadFileToString(const base::FilePath& path, std::string* data) {
  DCHECK(data);
  bool success = base::ReadFileToString(path, data);
  DLOG_IF(ERROR, !success) << "Error reading file " << path.LossyDisplayName();
  return success;
}

}  // namespace

namespace ntp_snippets {

NTPSnippetsService::NTPSnippetsService(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    const std::string& application_language_code,
    NTPSnippetsScheduler* scheduler,
    scoped_ptr<NTPSnippetsFetcher> snippets_fetcher)
    : loaded_(false),
      file_task_runner_(file_task_runner),
      application_language_code_(application_language_code),
      scheduler_(scheduler),
      snippets_fetcher_(std::move(snippets_fetcher)),
      weak_ptr_factory_(this) {
  snippets_fetcher_callback_ = snippets_fetcher_->AddCallback(
      base::Bind(&NTPSnippetsService::OnSnippetsDownloaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

NTPSnippetsService::~NTPSnippetsService() {}

void NTPSnippetsService::Init(bool enabled) {
  // If enabled, get snippets immediately. If we've downloaded them before,
  // this will just read from disk.
  if (enabled)
    FetchSnippets(false);

  // The scheduler only exists on Android so far, it's null on other platforms.
  if (!scheduler_)
    return;

  if (enabled)
    scheduler_->Schedule(kDefaultFetchingIntervalSeconds);
  else
    scheduler_->Unschedule();
}

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
  scoped_ptr<base::Value> deserialized = base::JSONReader::Read(str);
  if (!deserialized)
    return false;

  const base::DictionaryValue* top_dict = nullptr;
  if (!deserialized->GetAsDictionary(&top_dict))
    return false;

  const base::ListValue* list = nullptr;
  if (!top_dict->GetList("recos", &list))
    return false;

  for (const base::Value* const value : *list) {
    const base::DictionaryValue* dict = nullptr;
    if (!value->GetAsDictionary(&dict))
      return false;

    const base::DictionaryValue* content = nullptr;
    if (!dict->GetDictionary("contentInfo", &content))
      return false;
    scoped_ptr<NTPSnippet> snippet =
        NTPSnippet::NTPSnippetFromDictionary(*content);
    if (!snippet)
      return false;

    // Check if we already have a snippet with the same URL. If so, replace it
    // rather than adding a duplicate.
    const GURL& url = snippet->url();
    auto it = std::find_if(snippets_.begin(), snippets_.end(),
                           [&url](const scoped_ptr<NTPSnippet>& old_snippet) {
      return old_snippet->url() == url;
    });
    if (it != snippets_.end())
      *it = std::move(snippet);
    else
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

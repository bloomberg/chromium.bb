// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_service.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/suggestions/proto/suggestions.pb.h"

using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsService;

namespace ntp_snippets {

namespace {

const int kFetchingIntervalWifiChargingSeconds = 30 * 60;
const int kFetchingIntervalWifiSeconds = 2 * 60 * 60;
const int kFetchingIntervalFallbackSeconds = 24 * 60 * 60;

const int kDefaultExpiryTimeMins = 4 * 60;

base::TimeDelta GetFetchingInterval(const char* switch_name,
                                    int default_value_seconds) {
  int value_seconds = default_value_seconds;
  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  if (cmdline.HasSwitch(switch_name)) {
    std::string str = cmdline.GetSwitchValueASCII(switch_name);
    int switch_value_seconds = 0;
    if (base::StringToInt(str, &switch_value_seconds))
      value_seconds = switch_value_seconds;
    else
      LOG(WARNING) << "Invalid value for switch " << switch_name;
  }
  return base::TimeDelta::FromSeconds(value_seconds);
}

base::TimeDelta GetFetchingIntervalWifiCharging() {
  return GetFetchingInterval(switches::kFetchingIntervalWifiChargingSeconds,
                             kFetchingIntervalWifiChargingSeconds);
}

base::TimeDelta GetFetchingIntervalWifi() {
  return GetFetchingInterval(switches::kFetchingIntervalWifiSeconds,
                             kFetchingIntervalWifiSeconds);
}

base::TimeDelta GetFetchingIntervalFallback() {
  return GetFetchingInterval(switches::kFetchingIntervalFallbackSeconds,
                             kFetchingIntervalFallbackSeconds);
}

bool ReadFileToString(const base::FilePath& path, std::string* data) {
  DCHECK(data);
  bool success = base::ReadFileToString(path, data);
  DLOG_IF(ERROR, !success) << "Error reading file " << path.LossyDisplayName();
  return success;
}

std::vector<std::string> GetSuggestionsHosts(
    const SuggestionsProfile& suggestions) {
  std::vector<std::string> hosts;
  for (int i = 0; i < suggestions.suggestions_size(); ++i) {
    const ChromeSuggestion& suggestion = suggestions.suggestions(i);
    GURL url(suggestion.url());
    if (url.is_valid())
      hosts.push_back(url.host());
  }
  return hosts;
}

const char kContentInfo[] = "contentInfo";

}  // namespace

NTPSnippetsService::NTPSnippetsService(
    PrefService* pref_service,
    SuggestionsService* suggestions_service,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    const std::string& application_language_code,
    NTPSnippetsScheduler* scheduler,
    scoped_ptr<NTPSnippetsFetcher> snippets_fetcher)
    : pref_service_(pref_service),
      suggestions_service_(suggestions_service),
      loaded_(false),
      file_task_runner_(file_task_runner),
      application_language_code_(application_language_code),
      scheduler_(scheduler),
      snippets_fetcher_(std::move(snippets_fetcher)),
      weak_ptr_factory_(this) {
  snippets_fetcher_callback_ = snippets_fetcher_->AddCallback(base::Bind(
      &NTPSnippetsService::OnSnippetsDownloaded, base::Unretained(this)));
}

NTPSnippetsService::~NTPSnippetsService() {}

// static
void NTPSnippetsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kSnippets);
}

void NTPSnippetsService::Init(bool enabled) {
  if (enabled) {
    // |suggestions_service_| can be null in tests.
    if (suggestions_service_) {
      suggestions_service_subscription_ = suggestions_service_->AddCallback(
          base::Bind(&NTPSnippetsService::OnSuggestionsChanged,
                     base::Unretained(this)));
    }

    // Get any existing snippets immediately from prefs.
    LoadFromPrefs();

    // If we don't have any snippets yet, start a fetch.
    if (snippets_.empty())
      FetchSnippets();
  }

  // The scheduler only exists on Android so far, it's null on other platforms.
  if (!scheduler_)
    return;

  if (enabled) {
    scheduler_->Schedule(GetFetchingIntervalWifiCharging(),
                         GetFetchingIntervalWifi(),
                         GetFetchingIntervalFallback());
  } else {
    scheduler_->Unschedule();
  }
}

void NTPSnippetsService::Shutdown() {
  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceShutdown(this));
  loaded_ = false;
}

void NTPSnippetsService::FetchSnippets() {
  std::vector<std::string> hosts;
  // |suggestions_service_| can be null in tests.
  if (suggestions_service_) {
    hosts = GetSuggestionsHosts(
        suggestions_service_->GetSuggestionsDataFromCache());
  }
  snippets_fetcher_->FetchSnippets(hosts);
}

void NTPSnippetsService::AddObserver(NTPSnippetsServiceObserver* observer) {
  observers_.AddObserver(observer);
  if (loaded_)
    observer->NTPSnippetsServiceLoaded(this);
}

void NTPSnippetsService::RemoveObserver(NTPSnippetsServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NTPSnippetsService::OnSuggestionsChanged(
    const SuggestionsProfile& suggestions) {
  std::vector<std::string> hosts = GetSuggestionsHosts(suggestions);

  // Remove existing snippets that aren't in the suggestions anymore.
  snippets_.erase(
      std::remove_if(snippets_.begin(), snippets_.end(),
                     [&hosts](const scoped_ptr<NTPSnippet>& snippet) {
                       return std::find(hosts.begin(), hosts.end(),
                                        snippet->url().host()) == hosts.end();
                     }),
      snippets_.end());

  StoreToPrefs();

  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceLoaded(this));

  snippets_fetcher_->FetchSnippets(hosts);
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

  return LoadFromJSONList(*list);
}

bool NTPSnippetsService::LoadFromJSONList(const base::ListValue& list) {
  for (const base::Value* const value : list) {
    const base::DictionaryValue* dict = nullptr;
    if (!value->GetAsDictionary(&dict))
      return false;

    const base::DictionaryValue* content = nullptr;
    if (!dict->GetDictionary(kContentInfo, &content))
      return false;
    scoped_ptr<NTPSnippet> snippet = NTPSnippet::CreateFromDictionary(*content);
    if (!snippet)
      return false;

    // If the snippet has no publish/expiry dates, fill in defaults.
    if (snippet->publish_date().is_null())
      snippet->set_publish_date(base::Time::Now());
    if (snippet->expiry_date().is_null()) {
      snippet->set_expiry_date(
          snippet->publish_date() +
          base::TimeDelta::FromMinutes(kDefaultExpiryTimeMins));
    }

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

  // Immediately remove any already-expired snippets. This will also notify our
  // observers and schedule the expiry timer.
  RemoveExpiredSnippets();

  return true;
}

void NTPSnippetsService::LoadFromPrefs() {
  // |pref_service_| can be null in tests.
  if (!pref_service_)
    return;
  if (!LoadFromJSONList(*pref_service_->GetList(prefs::kSnippets)))
    LOG(ERROR) << "Failed to parse snippets from prefs";
}

void NTPSnippetsService::StoreToPrefs() {
  // |pref_service_| can be null in tests.
  if (!pref_service_)
    return;
  base::ListValue list;
  for (const auto& snippet : snippets_) {
    scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    dict->Set(kContentInfo, snippet->ToDictionary());
    list.Append(std::move(dict));
  }
  pref_service_->Set(prefs::kSnippets, list);
}

void NTPSnippetsService::RemoveExpiredSnippets() {
  base::Time expiry = base::Time::Now();
  snippets_.erase(
      std::remove_if(snippets_.begin(), snippets_.end(),
                     [&expiry](const scoped_ptr<NTPSnippet>& snippet) {
                       return snippet->expiry_date() <= expiry;
                     }),
      snippets_.end());

  StoreToPrefs();

  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceLoaded(this));

  // If there are any snippets left, schedule a timer for the next expiry.
  if (snippets_.empty())
    return;

  base::Time next_expiry = base::Time::Max();
  for (const auto& snippet : snippets_) {
    if (snippet->expiry_date() < next_expiry)
      next_expiry = snippet->expiry_date();
  }
  DCHECK_GT(next_expiry, expiry);
  expiry_timer_.Start(FROM_HERE, next_expiry - expiry,
                      base::Bind(&NTPSnippetsService::RemoveExpiredSnippets,
                                 base::Unretained(this)));
}

}  // namespace ntp_snippets

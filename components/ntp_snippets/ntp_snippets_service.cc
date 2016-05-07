// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_service.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/suggestions/proto/suggestions.pb.h"

using image_fetcher::ImageFetcher;
using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsService;

namespace ntp_snippets {

namespace {

// Number of snippets requested to the server. Consider replacing sparse UMA
// histograms with COUNTS() if this number increases beyond 50.
const int kMaxSnippetCount = 10;

const int kFetchingIntervalWifiChargingSeconds = 30 * 60;
const int kFetchingIntervalWifiSeconds = 2 * 60 * 60;
const int kFetchingIntervalFallbackSeconds = 24 * 60 * 60;

// These define the times of day during which we will fetch via Wifi (without
// charging) - 6 AM to 10 PM.
const int kWifiFetchingHourMin = 6;
const int kWifiFetchingHourMax = 22;

const int kDefaultExpiryTimeMins = 24 * 60;

const char kStatusMessageEmptyHosts[] = "Cannot fetch for empty hosts list.";
const char kStatusMessageOK[] = "OK";

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

base::TimeDelta GetFetchingIntervalWifi(const base::Time& now) {
  // Only fetch via Wifi (without charging) during the proper times of day.
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  if (kWifiFetchingHourMin <= exploded.hour &&
      exploded.hour < kWifiFetchingHourMax) {
    return GetFetchingInterval(switches::kFetchingIntervalWifiSeconds,
                               kFetchingIntervalWifiSeconds);
  }
  return base::TimeDelta();
}

base::TimeDelta GetFetchingIntervalFallback() {
  return GetFetchingInterval(switches::kFetchingIntervalFallbackSeconds,
                             kFetchingIntervalFallbackSeconds);
}

base::Time GetRescheduleTime(const base::Time& now) {
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  // The scheduling changes at both |kWifiFetchingHourMin| and
  // |kWifiFetchingHourMax|. Find the time of the next one that we'll hit.
  bool next_day = false;
  if (exploded.hour < kWifiFetchingHourMin) {
    exploded.hour = kWifiFetchingHourMin;
  } else if (exploded.hour < kWifiFetchingHourMax) {
    exploded.hour = kWifiFetchingHourMax;
  } else {
    next_day = true;
    exploded.hour = kWifiFetchingHourMin;
  }
  // In any case, reschedule at the full hour.
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  base::Time reschedule = base::Time::FromLocalExploded(exploded);
  if (next_day)
    reschedule += base::TimeDelta::FromDays(1);

  return reschedule;
}

// Extracts the hosts from |suggestions| and returns them in a set.
std::set<std::string> GetSuggestionsHostsImpl(
    const SuggestionsProfile& suggestions) {
  std::set<std::string> hosts;
  for (int i = 0; i < suggestions.suggestions_size(); ++i) {
    const ChromeSuggestion& suggestion = suggestions.suggestions(i);
    GURL url(suggestion.url());
    if (url.is_valid())
      hosts.insert(url.host());
  }
  return hosts;
}

std::unique_ptr<base::ListValue> SnippetsToListValue(
    const NTPSnippetsService::NTPSnippetStorage& snippets) {
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  for (const auto& snippet : snippets) {
    std::unique_ptr<base::DictionaryValue> dict = snippet->ToDictionary();
    list->Append(std::move(dict));
  }
  return list;
}

bool ContainsSnippet(const NTPSnippetsService::NTPSnippetStorage& haystack,
                     const std::unique_ptr<NTPSnippet>& needle) {
  const GURL& url = needle->url();
  return std::find_if(haystack.begin(), haystack.end(),
                      [&url](const std::unique_ptr<NTPSnippet>& snippet) {
                        return snippet->url() == url;
                      }) != haystack.end();
}

}  // namespace

NTPSnippetsService::NTPSnippetsService(
    PrefService* pref_service,
    SuggestionsService* suggestions_service,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    const std::string& application_language_code,
    NTPSnippetsScheduler* scheduler,
    std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher,
    std::unique_ptr<ImageFetcher> image_fetcher)
    : enabled_(false),
      pref_service_(pref_service),
      suggestions_service_(suggestions_service),
      file_task_runner_(file_task_runner),
      application_language_code_(application_language_code),
      scheduler_(scheduler),
      snippets_fetcher_(std::move(snippets_fetcher)),
      image_fetcher_(std::move(image_fetcher)) {
  snippets_fetcher_->SetCallback(base::Bind(
      &NTPSnippetsService::OnFetchFinished, base::Unretained(this)));
}

NTPSnippetsService::~NTPSnippetsService() {}

// static
void NTPSnippetsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kSnippets);
  registry->RegisterListPref(prefs::kDiscardedSnippets);
  registry->RegisterListPref(prefs::kSnippetHosts);
}

void NTPSnippetsService::Init(bool enabled) {
  enabled_ = enabled;
  if (enabled_) {
    // |suggestions_service_| can be null in tests.
    if (suggestions_service_) {
      suggestions_service_subscription_ = suggestions_service_->AddCallback(
          base::Bind(&NTPSnippetsService::OnSuggestionsChanged,
                     base::Unretained(this)));
    }

    // Get any existing snippets immediately from prefs.
    LoadDiscardedSnippetsFromPrefs();
    LoadSnippetsFromPrefs();

    // If we don't have any snippets yet, start a fetch.
    if (snippets_.empty())
      FetchSnippets();
  }

  RescheduleFetching();
}

void NTPSnippetsService::Shutdown() {
  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceShutdown());
  enabled_ = false;
}

void NTPSnippetsService::FetchSnippets() {
  FetchSnippetsFromHosts(GetSuggestionsHosts());
}

void NTPSnippetsService::FetchSnippetsFromHosts(
    const std::set<std::string>& hosts) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDontRestrict)) {
    snippets_fetcher_->FetchSnippets(std::set<std::string>(), kMaxSnippetCount);
    return;
  }
  if (!hosts.empty()) {
    snippets_fetcher_->FetchSnippets(hosts, kMaxSnippetCount);
  } else {
    last_fetch_status_ = kStatusMessageEmptyHosts;
    LoadingSnippetsFinished();
  }
}

void NTPSnippetsService::RescheduleFetching() {
  // The scheduler only exists on Android so far, it's null on other platforms.
  if (!scheduler_)
    return;

  if (enabled_) {
    base::Time now = base::Time::Now();
    scheduler_->Schedule(
        GetFetchingIntervalWifiCharging(), GetFetchingIntervalWifi(now),
        GetFetchingIntervalFallback(), GetRescheduleTime(now));
  } else {
    scheduler_->Unschedule();
  }
}

void NTPSnippetsService::FetchSnippetImage(
    const GURL& url,
    const ImageFetchedCallback& callback) {
  auto it = std::find_if(snippets_.begin(), snippets_.end(),
                         [&url](const std::unique_ptr<NTPSnippet>& snippet) {
                           return snippet->url() == url;
                         });
  if (it == snippets_.end()) {
    callback.Run(url, nullptr);
    return;
  }

  const NTPSnippet& snippet = *it->get();
  image_fetcher_->StartOrQueueNetworkRequest(
      snippet.url(), snippet.salient_image_url(), callback);
  // TODO(treib): Cache/persist the snippet image.
}

void NTPSnippetsService::ClearSnippets() {
  snippets_.clear();

  StoreSnippetsToPrefs();

  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceLoaded());
}

std::set<std::string> NTPSnippetsService::GetSuggestionsHosts() const {
  // |suggestions_service_| can be null in tests.
  if (!suggestions_service_)
    return std::set<std::string>();

  // TODO(treib) this should just call GetSnippetHostsFromPrefs
  return GetSuggestionsHostsImpl(
      suggestions_service_->GetSuggestionsDataFromCache());
}

bool NTPSnippetsService::DiscardSnippet(const GURL& url) {
  auto it = std::find_if(snippets_.begin(), snippets_.end(),
                         [&url](const std::unique_ptr<NTPSnippet>& snippet) {
                           return snippet->url() == url ||
                                  snippet->best_source().url == url;
                         });
  if (it == snippets_.end())
    return false;
  discarded_snippets_.push_back(std::move(*it));
  snippets_.erase(it);
  StoreDiscardedSnippetsToPrefs();
  StoreSnippetsToPrefs();
  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceLoaded());
  return true;
}

void NTPSnippetsService::ClearDiscardedSnippets() {
  discarded_snippets_.clear();
  StoreDiscardedSnippetsToPrefs();
  FetchSnippets();
}

void NTPSnippetsService::AddObserver(NTPSnippetsServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void NTPSnippetsService::RemoveObserver(NTPSnippetsServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

// static
int NTPSnippetsService::GetMaxSnippetCountForTesting() {
  return kMaxSnippetCount;
}

void NTPSnippetsService::OnSuggestionsChanged(
    const SuggestionsProfile& suggestions) {
  std::set<std::string> hosts = GetSuggestionsHostsImpl(suggestions);
  if (hosts == GetSnippetHostsFromPrefs())
    return;

  // Remove existing snippets that aren't in the suggestions anymore.
  snippets_.erase(
      std::remove_if(snippets_.begin(), snippets_.end(),
                     [&hosts](const std::unique_ptr<NTPSnippet>& snippet) {
                       return !hosts.count(snippet->url().host());
                     }),
      snippets_.end());

  StoreSnippetsToPrefs();
  StoreSnippetHostsToPrefs(hosts);

  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceLoaded());

  FetchSnippetsFromHosts(hosts);
}

void NTPSnippetsService::OnFetchFinished(NTPSnippetStorage snippets,
                                         const std::string& status) {
  if (!status.empty()) {
    last_fetch_status_ = status;
  } else {
    last_fetch_status_ = kStatusMessageOK;
    // Sparse histogram used because the number of snippets is small (bound by
    // kMaxSnippetCount).
    DCHECK_LE(snippets.size(), static_cast<size_t>(kMaxSnippetCount));
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumArticlesFetched",
                                snippets.size());
    MergeSnippets(std::move(snippets));
  }
  LoadingSnippetsFinished();
}

void NTPSnippetsService::MergeSnippets(NTPSnippetStorage new_snippets) {
  // Remove new snippets that we already have, or that have been discarded.
  new_snippets.erase(
      std::remove_if(new_snippets.begin(), new_snippets.end(),
                     [this](const std::unique_ptr<NTPSnippet>& snippet) {
                       return ContainsSnippet(discarded_snippets_, snippet) ||
                              ContainsSnippet(snippets_, snippet);
                     }),
      new_snippets.end());

  // Fill in default publish/expiry dates where required.
  for (std::unique_ptr<NTPSnippet>& snippet : new_snippets) {
    if (snippet->publish_date().is_null())
      snippet->set_publish_date(base::Time::Now());
    if (snippet->expiry_date().is_null()) {
      snippet->set_expiry_date(
          snippet->publish_date() +
          base::TimeDelta::FromMinutes(kDefaultExpiryTimeMins));
    }

    // TODO(treib): Prefetch and cache the snippet image. crbug.com/605870
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAddIncompleteSnippets)) {
    int num_new_snippets = new_snippets.size();
    // Remove snippets that do not have all the info we need to display it to
    // the user.
    new_snippets.erase(
        std::remove_if(new_snippets.begin(), new_snippets.end(),
                       [](const std::unique_ptr<NTPSnippet>& snippet) {
                         return !snippet->is_complete();
                       }),
        new_snippets.end());
    int num_snippets_discarded = num_new_snippets - new_snippets.size();
    UMA_HISTOGRAM_BOOLEAN("NewTabPage.Snippets.IncompleteSnippetsAfterFetch",
                          num_snippets_discarded > 0);
    if (num_snippets_discarded > 0) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumIncompleteSnippets",
                                  num_snippets_discarded);
    }
  }

  // Insert the new snippets at the front.
  snippets_.insert(snippets_.begin(),
                   std::make_move_iterator(new_snippets.begin()),
                   std::make_move_iterator(new_snippets.end()));
}

void NTPSnippetsService::LoadSnippetsFromPrefs() {
  NTPSnippetStorage prefs_snippets;
  bool success = NTPSnippet::AddFromListValue(
      *pref_service_->GetList(prefs::kSnippets), &prefs_snippets);
  DCHECK(success) << "Failed to parse snippets from prefs";
  MergeSnippets(std::move(prefs_snippets));
  LoadingSnippetsFinished();
}

void NTPSnippetsService::StoreSnippetsToPrefs() {
  pref_service_->Set(prefs::kSnippets, *SnippetsToListValue(snippets_));
}

void NTPSnippetsService::LoadDiscardedSnippetsFromPrefs() {
  discarded_snippets_.clear();
  bool success = NTPSnippet::AddFromListValue(
      *pref_service_->GetList(prefs::kDiscardedSnippets), &discarded_snippets_);
  DCHECK(success) << "Failed to parse discarded snippets from prefs";
}

void NTPSnippetsService::StoreDiscardedSnippetsToPrefs() {
  pref_service_->Set(prefs::kDiscardedSnippets,
                     *SnippetsToListValue(discarded_snippets_));
}

std::set<std::string> NTPSnippetsService::GetSnippetHostsFromPrefs() const {
  std::set<std::string> hosts;
  const base::ListValue* list = pref_service_->GetList(prefs::kSnippetHosts);
  for (const base::Value* value : *list) {
    std::string str;
    bool success = value->GetAsString(&str);
    DCHECK(success) << "Failed to parse snippet host from prefs";
    hosts.insert(std::move(str));
  }
  return hosts;
}

void NTPSnippetsService::StoreSnippetHostsToPrefs(
    const std::set<std::string>& hosts) {
  base::ListValue list;
  for (const std::string& host : hosts)
    list.AppendString(host);
  pref_service_->Set(prefs::kSnippetHosts, list);
}

void NTPSnippetsService::LoadingSnippetsFinished() {
  // Remove expired snippets.
  base::Time expiry = base::Time::Now();

  snippets_.erase(
      std::remove_if(snippets_.begin(), snippets_.end(),
                     [&expiry](const std::unique_ptr<NTPSnippet>& snippet) {
                       return snippet->expiry_date() <= expiry;
                     }),
      snippets_.end());

  // If there are more snippets now than we want to show, drop the extra ones
  // from the end of the list.
  if (snippets_.size() > kMaxSnippetCount)
    snippets_.resize(kMaxSnippetCount);

  StoreSnippetsToPrefs();

  discarded_snippets_.erase(
      std::remove_if(discarded_snippets_.begin(), discarded_snippets_.end(),
                     [&expiry](const std::unique_ptr<NTPSnippet>& snippet) {
                       return snippet->expiry_date() <= expiry;
                     }),
      discarded_snippets_.end());
  StoreDiscardedSnippetsToPrefs();

  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumArticles",
                              snippets_.size());
  if (snippets_.empty() && !discarded_snippets_.empty()) {
    UMA_HISTOGRAM_COUNTS("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded",
                         discarded_snippets_.size());
  }

  FOR_EACH_OBSERVER(NTPSnippetsServiceObserver, observers_,
                    NTPSnippetsServiceLoaded());

  // If there are any snippets left, schedule a timer for the next expiry.
  if (snippets_.empty() && discarded_snippets_.empty())
    return;

  base::Time next_expiry = base::Time::Max();
  for (const auto& snippet : snippets_) {
    if (snippet->expiry_date() < next_expiry)
      next_expiry = snippet->expiry_date();
  }
  for (const auto& snippet : discarded_snippets_) {
    if (snippet->expiry_date() < next_expiry)
      next_expiry = snippet->expiry_date();
  }
  DCHECK_GT(next_expiry, expiry);
  expiry_timer_.Start(FROM_HERE, next_expiry - expiry,
                      base::Bind(&NTPSnippetsService::LoadingSnippetsFinished,
                                 base::Unretained(this)));
}

}  // namespace ntp_snippets

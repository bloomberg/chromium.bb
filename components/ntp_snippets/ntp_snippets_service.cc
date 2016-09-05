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
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/history/core/browser/history_service.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/ntp_snippets_database.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using image_fetcher::ImageDecoder;
using image_fetcher::ImageFetcher;
using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsService;

namespace ntp_snippets {

namespace {

// Number of snippets requested to the server. Consider replacing sparse UMA
// histograms with COUNTS() if this number increases beyond 50.
const int kMaxSnippetCount = 10;

// Default values for snippets fetching intervals - once per day only.
const int kDefaultFetchingIntervalWifiChargingSeconds = 0;
const int kDefaultFetchingIntervalWifiSeconds = 0;
const int kDefaultFetchingIntervalFallbackSeconds = 24 * 60 * 60;

// Variation parameters than can override the default fetching intervals.
const char kFetchingIntervalWifiChargingParamName[] =
    "fetching_interval_wifi_charging_seconds";
const char kFetchingIntervalWifiParamName[] =
    "fetching_interval_wifi_seconds";
const char kFetchingIntervalFallbackParamName[] =
    "fetching_interval_fallback_seconds";

// These define the times of day during which we will fetch via Wifi (without
// charging) - 6 AM to 10 PM.
const int kWifiFetchingHourMin = 6;
const int kWifiFetchingHourMax = 22;

const int kDefaultExpiryTimeMins = 24 * 60;

base::TimeDelta GetFetchingInterval(const char* switch_name,
                                    const char* param_name,
                                    int default_value_seconds) {
  int value_seconds = default_value_seconds;

  // The default value can be overridden by a variation parameter.
  // TODO(treib,jkrcal): Use GetVariationParamValueByFeature and get rid of
  // kStudyName, also in NTPSnippetsFetcher.
  std::string param_value_str = variations::GetVariationParamValue(
        ntp_snippets::kStudyName, param_name);
  if (!param_value_str.empty()) {
    int param_value_seconds = 0;
    if (base::StringToInt(param_value_str, &param_value_seconds))
      value_seconds = param_value_seconds;
    else
      LOG(WARNING) << "Invalid value for variation parameter " << param_name;
  }

  // A value from the command line parameter overrides anything else.
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
                             kFetchingIntervalWifiChargingParamName,
                             kDefaultFetchingIntervalWifiChargingSeconds);
}

base::TimeDelta GetFetchingIntervalWifi(const base::Time& now) {
  // Only fetch via Wifi (without charging) during the proper times of day.
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  if (kWifiFetchingHourMin <= exploded.hour &&
      exploded.hour < kWifiFetchingHourMax) {
    return GetFetchingInterval(switches::kFetchingIntervalWifiSeconds,
                               kFetchingIntervalWifiParamName,
                               kDefaultFetchingIntervalWifiSeconds);
  }
  return base::TimeDelta();
}

base::TimeDelta GetFetchingIntervalFallback() {
  return GetFetchingInterval(switches::kFetchingIntervalFallbackSeconds,
                             kFetchingIntervalFallbackParamName,
                             kDefaultFetchingIntervalFallbackSeconds);
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
  base::Time reschedule;
  if (!base::Time::FromLocalExploded(exploded, &reschedule)) {
    return GetRescheduleTime(now + base::TimeDelta::FromDays(1));
  }

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

void InsertAllIDs(const NTPSnippet::PtrVector& snippets,
                  std::set<std::string>* ids) {
  for (const std::unique_ptr<NTPSnippet>& snippet : snippets) {
    ids->insert(snippet->id());
    for (const SnippetSource& source : snippet->sources())
      ids->insert(source.url.spec());
  }
}

void Compact(NTPSnippet::PtrVector* snippets) {
  snippets->erase(
      std::remove_if(
          snippets->begin(), snippets->end(),
          [](const std::unique_ptr<NTPSnippet>& snippet) { return !snippet; }),
      snippets->end());
}

}  // namespace

NTPSnippetsService::NTPSnippetsService(
    Observer* observer,
    CategoryFactory* category_factory,
    PrefService* pref_service,
    SuggestionsService* suggestions_service,
    const std::string& application_language_code,
    NTPSnippetsScheduler* scheduler,
    std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher,
    std::unique_ptr<ImageFetcher> image_fetcher,
    std::unique_ptr<ImageDecoder> image_decoder,
    std::unique_ptr<NTPSnippetsDatabase> database,
    std::unique_ptr<NTPSnippetsStatusService> status_service)
    : ContentSuggestionsProvider(observer, category_factory),
      state_(State::NOT_INITED),
      pref_service_(pref_service),
      suggestions_service_(suggestions_service),
      articles_category_(
          category_factory->FromKnownCategory(KnownCategories::ARTICLES)),
      application_language_code_(application_language_code),
      scheduler_(scheduler),
      snippets_fetcher_(std::move(snippets_fetcher)),
      image_fetcher_(std::move(image_fetcher)),
      image_decoder_(std::move(image_decoder)),
      database_(std::move(database)),
      snippets_status_service_(std::move(status_service)),
      fetch_after_load_(false),
      nuke_after_load_(false),
      thumbnail_requests_throttler_(
          pref_service,
          RequestThrottler::RequestType::CONTENT_SUGGESTION_THUMBNAIL) {
  // Articles category always exists; others will be added as needed.
  categories_[articles_category_] = CategoryContent();
  observer->OnCategoryStatusChanged(this, articles_category_,
                                    categories_[articles_category_].status);
  if (database_->IsErrorState()) {
    EnterState(State::ERROR_OCCURRED);
    UpdateAllCategoryStatus(CategoryStatus::LOADING_ERROR);
    return;
  }

  database_->SetErrorCallback(base::Bind(&NTPSnippetsService::OnDatabaseError,
                                         base::Unretained(this)));

  // We transition to other states while finalizing the initialization, when the
  // database is done loading.
  database_->LoadSnippets(base::Bind(&NTPSnippetsService::OnDatabaseLoaded,
                                     base::Unretained(this)));
}

NTPSnippetsService::~NTPSnippetsService() {
}

// static
void NTPSnippetsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kSnippetHosts);

  NTPSnippetsStatusService::RegisterProfilePrefs(registry);
}

void NTPSnippetsService::FetchSnippets(bool interactive_request) {
  if (ready())
    FetchSnippetsFromHosts(GetSuggestionsHosts(), interactive_request);
  else
    fetch_after_load_ = true;
}

void NTPSnippetsService::FetchSnippetsFromHosts(
    const std::set<std::string>& hosts,
    bool interactive_request) {
  if (!ready())
    return;

  // Empty categories are marked as loading; others are unchanged.
  for (const auto& item : categories_) {
    Category category = item.first;
    const CategoryContent& content = item.second;
    if (content.snippets.empty())
      UpdateCategoryStatus(category, CategoryStatus::AVAILABLE_LOADING);
  }

  std::set<std::string> excluded_ids;
  for (const auto& item : categories_) {
    const CategoryContent& content = item.second;
    for (const auto& snippet : content.dismissed)
      excluded_ids.insert(snippet->id());
  }
  snippets_fetcher_->FetchSnippetsFromHosts(hosts, application_language_code_,
                                            excluded_ids, kMaxSnippetCount,
                                            interactive_request);
}

void NTPSnippetsService::RescheduleFetching() {
  // The scheduler only exists on Android so far, it's null on other platforms.
  if (!scheduler_)
    return;

  if (ready()) {
    base::Time now = base::Time::Now();
    scheduler_->Schedule(
        GetFetchingIntervalWifiCharging(), GetFetchingIntervalWifi(now),
        GetFetchingIntervalFallback(), GetRescheduleTime(now));
  } else {
    scheduler_->Unschedule();
  }
}

CategoryStatus NTPSnippetsService::GetCategoryStatus(Category category) {
  DCHECK(categories_.find(category) != categories_.end());
  return categories_[category].status;
}

CategoryInfo NTPSnippetsService::GetCategoryInfo(Category category) {
  DCHECK(categories_.find(category) != categories_.end());
  // TODO(sfiera): pass back titles for server categories.
  return CategoryInfo(
      l10n_util::GetStringUTF16(IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_HEADER),
      ContentSuggestionsCardLayout::FULL_CARD,
      /* has_more_button */ false,
      /* show_if_empty */ true);
}

void NTPSnippetsService::DismissSuggestion(const std::string& suggestion_id) {
  if (!ready())
    return;

  Category category = GetCategoryFromUniqueID(suggestion_id);
  std::string snippet_id = GetWithinCategoryIDFromUniqueID(suggestion_id);

  DCHECK(categories_.find(category) != categories_.end());

  CategoryContent* content = &categories_[category];
  auto it =
      std::find_if(content->snippets.begin(), content->snippets.end(),
                   [&snippet_id](const std::unique_ptr<NTPSnippet>& snippet) {
                     return snippet->id() == snippet_id;
                   });
  if (it == content->snippets.end())
    return;

  (*it)->set_dismissed(true);

  database_->SaveSnippet(**it);
  database_->DeleteImage(snippet_id);

  content->dismissed.push_back(std::move(*it));
  content->snippets.erase(it);
}

void NTPSnippetsService::FetchSuggestionImage(
    const std::string& suggestion_id,
    const ImageFetchedCallback& callback) {
  std::string snippet_id = GetWithinCategoryIDFromUniqueID(suggestion_id);
  database_->LoadImage(
      snippet_id,
      base::Bind(&NTPSnippetsService::OnSnippetImageFetchedFromDatabase,
                 base::Unretained(this), callback, suggestion_id));
}

void NTPSnippetsService::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  // Both time range and the filter are ignored and all suggestions are removed,
  // because it is not known which history entries were used for the suggestions
  // personalization.
  if (!ready())
    nuke_after_load_ = true;
  else
    NukeAllSnippets();
}

void NTPSnippetsService::ClearCachedSuggestions(Category category) {
  if (!initialized())
    return;

  if (categories_.find(category) == categories_.end())
    return;
  CategoryContent* content = &categories_[category];
  if (content->snippets.empty())
    return;

  if (category == articles_category_)
    database_->DeleteSnippets(content->snippets);
  content->snippets.clear();

  NotifyNewSuggestions();
}

void NTPSnippetsService::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  DCHECK(categories_.find(category) != categories_.end());

  std::vector<ContentSuggestion> result;
  const CategoryContent& content = categories_[category];
  for (const std::unique_ptr<NTPSnippet>& snippet : content.dismissed) {
    if (!snippet->is_complete())
      continue;
    ContentSuggestion suggestion(MakeUniqueID(category, snippet->id()),
                                 snippet->best_source().url);
    suggestion.set_amp_url(snippet->best_source().amp_url);
    suggestion.set_title(base::UTF8ToUTF16(snippet->title()));
    suggestion.set_snippet_text(base::UTF8ToUTF16(snippet->snippet()));
    suggestion.set_publish_date(snippet->publish_date());
    suggestion.set_publisher_name(
        base::UTF8ToUTF16(snippet->best_source().publisher_name));
    suggestion.set_score(snippet->score());
    result.emplace_back(std::move(suggestion));
  }
  callback.Run(std::move(result));
}

void NTPSnippetsService::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK(categories_.find(category) != categories_.end());

  if (!initialized())
    return;

  CategoryContent* content = &categories_[category];
  if (content->dismissed.empty())
    return;

  if (category == articles_category_)
    database_->DeleteSnippets(content->dismissed);
  content->dismissed.clear();
}

std::set<std::string> NTPSnippetsService::GetSuggestionsHosts() const {
  // |suggestions_service_| can be null in tests.
  if (!suggestions_service_)
    return std::set<std::string>();

  // TODO(treib): This should just call GetSnippetHostsFromPrefs.
  return GetSuggestionsHostsImpl(
      suggestions_service_->GetSuggestionsDataFromCache());
}

// static
int NTPSnippetsService::GetMaxSnippetCountForTesting() {
  return kMaxSnippetCount;
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

// image_fetcher::ImageFetcherDelegate implementation.
void NTPSnippetsService::OnImageDataFetched(const std::string& suggestion_id,
                                            const std::string& image_data) {
  if (image_data.empty())
    return;

  Category category = GetCategoryFromUniqueID(suggestion_id);
  std::string snippet_id = GetWithinCategoryIDFromUniqueID(suggestion_id);

  auto category_it = categories_.find(category);
  if (category_it == categories_.end())
    return;

  const CategoryContent& content = category_it->second;

  // Only save the image if the corresponding snippet still exists.
  auto it =
      std::find_if(content.snippets.begin(), content.snippets.end(),
                   [&snippet_id](const std::unique_ptr<NTPSnippet>& snippet) {
                     return snippet->id() == snippet_id;
                   });
  if (it == content.snippets.end())
    return;

  database_->SaveImage(snippet_id, image_data);
}

void NTPSnippetsService::OnDatabaseLoaded(NTPSnippet::PtrVector snippets) {
  if (state_ == State::ERROR_OCCURRED)
    return;
  DCHECK(state_ == State::NOT_INITED);
  DCHECK(categories_.size() == 1);  // Only articles category, so far.
  DCHECK(categories_.find(articles_category_) != categories_.end());

  // TODO(sfiera): support non-article categories in database.
  CategoryContent* content = &categories_[articles_category_];
  for (std::unique_ptr<NTPSnippet>& snippet : snippets) {
    if (snippet->is_dismissed())
      content->dismissed.emplace_back(std::move(snippet));
    else
      content->snippets.emplace_back(std::move(snippet));
  }

  std::sort(content->snippets.begin(), content->snippets.end(),
            [](const std::unique_ptr<NTPSnippet>& lhs,
               const std::unique_ptr<NTPSnippet>& rhs) {
              return lhs->score() > rhs->score();
            });

  ClearExpiredSnippets();
  FinishInitialization();
}

void NTPSnippetsService::OnDatabaseError() {
  EnterState(State::ERROR_OCCURRED);
  UpdateAllCategoryStatus(CategoryStatus::LOADING_ERROR);
}

// TODO(dgn): name clash between content suggestions and suggestions hosts.
// method name should be changed.
void NTPSnippetsService::OnSuggestionsChanged(
    const SuggestionsProfile& suggestions) {
  DCHECK(initialized());

  std::set<std::string> hosts = GetSuggestionsHostsImpl(suggestions);
  if (hosts == GetSnippetHostsFromPrefs())
    return;

  // Remove existing snippets that aren't in the suggestions anymore.
  //
  // TODO(treib,maybelle): If there is another source with an allowed host,
  // then we should fall back to that.
  //
  // TODO(sfiera): determine when non-article categories should restrict hosts,
  // and apply the same logic to them here. Maybe never?
  //
  // First, move them over into |to_delete|.
  CategoryContent* content = &categories_[articles_category_];
  NTPSnippet::PtrVector to_delete;
  for (std::unique_ptr<NTPSnippet>& snippet : content->snippets) {
    if (!hosts.count(snippet->best_source().url.host()))
      to_delete.emplace_back(std::move(snippet));
  }
  Compact(&content->snippets);
  // Then delete the removed snippets from the database.
  database_->DeleteSnippets(to_delete);

  StoreSnippetHostsToPrefs(hosts);

  // We removed some suggestions, so we want to let the client know about that.
  // The fetch might take a long time or not complete so we don't want to wait
  // for its callback.
  NotifyNewSuggestions();

  FetchSnippetsFromHosts(hosts, /*force_request=*/false);
}

void NTPSnippetsService::OnFetchFinished(
    NTPSnippetsFetcher::OptionalSnippets snippets) {
  if (!ready())
    return;

  for (auto& item : categories_) {
    CategoryContent* content = &item.second;
    content->provided_by_server = false;
  }

  // If snippets were fetched successfully, update our |categories_| from each
  // category provided by the server.
  if (snippets) {
    for (std::pair<const Category, NTPSnippet::PtrVector>& item : *snippets) {
      Category category = item.first;
      NTPSnippet::PtrVector& new_snippets = item.second;

      DCHECK_LE(snippets->size(), static_cast<size_t>(kMaxSnippetCount));
      // TODO(sfiera): histograms for server categories.
      // Sparse histogram used because the number of snippets is small (bound by
      // kMaxSnippetCount).
      if (category == articles_category_) {
        UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumArticlesFetched",
                                    new_snippets.size());
      }

      MergeSnippets(category, std::move(new_snippets));

      // If there are more snippets than we want to show, delete the extra ones.
      CategoryContent* content = &categories_[category];
      content->provided_by_server = true;
      if (content->snippets.size() > kMaxSnippetCount) {
        NTPSnippet::PtrVector to_delete(
            std::make_move_iterator(content->snippets.begin() +
                                    kMaxSnippetCount),
            std::make_move_iterator(content->snippets.end()));
        content->snippets.resize(kMaxSnippetCount);
        if (category == articles_category_)
          database_->DeleteSnippets(to_delete);
      }
    }
  }

  // Trigger expiration.  This probably won't expire any current snippets (old
  // ones should have already been expired by the timer, and new ones shouldn't
  // have expired yet), but it will update the timer for the next run.
  ClearExpiredSnippets();

  for (const auto& item : categories_) {
    Category category = item.first;
    UpdateCategoryStatus(category, CategoryStatus::AVAILABLE);
  }

  // TODO(sfiera): equivalent metrics for non-articles.
  const CategoryContent& content = categories_[articles_category_];
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumArticles",
                              content.snippets.size());
  if (content.snippets.empty() && !content.dismissed.empty()) {
    UMA_HISTOGRAM_COUNTS("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded",
                         content.dismissed.size());
  }

  // TODO(sfiera): notify only when a category changed above.
  NotifyNewSuggestions();
}

void NTPSnippetsService::MergeSnippets(Category category,
                                       NTPSnippet::PtrVector new_snippets) {
  DCHECK(ready());
  CategoryContent* content = &categories_[category];

  // Remove new snippets that we already have, or that have been dismissed.
  std::set<std::string> old_snippet_ids;
  InsertAllIDs(content->dismissed, &old_snippet_ids);
  InsertAllIDs(content->snippets, &old_snippet_ids);
  new_snippets.erase(
      std::remove_if(
          new_snippets.begin(), new_snippets.end(),
          [&old_snippet_ids](const std::unique_ptr<NTPSnippet>& snippet) {
            if (old_snippet_ids.count(snippet->id()))
              return true;
            for (const SnippetSource& source : snippet->sources()) {
              if (old_snippet_ids.count(source.url.spec()))
                return true;
            }
            return false;
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
    int num_snippets_dismissed = num_new_snippets - new_snippets.size();
    UMA_HISTOGRAM_BOOLEAN("NewTabPage.Snippets.IncompleteSnippetsAfterFetch",
                          num_snippets_dismissed > 0);
    if (num_snippets_dismissed > 0) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumIncompleteSnippets",
                                  num_snippets_dismissed);
    }
  }

  // Save new articles to the DB.
  // TODO(sfiera): save non-articles to DB too.
  if (category == articles_category_)
    database_->SaveSnippets(new_snippets);

  // Insert the new snippets at the front.
  content->snippets.insert(content->snippets.begin(),
                           std::make_move_iterator(new_snippets.begin()),
                           std::make_move_iterator(new_snippets.end()));
}

std::set<std::string> NTPSnippetsService::GetSnippetHostsFromPrefs() const {
  std::set<std::string> hosts;
  const base::ListValue* list = pref_service_->GetList(prefs::kSnippetHosts);
  for (const auto& value : *list) {
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

void NTPSnippetsService::ClearExpiredSnippets() {
  std::vector<Category> categories_to_erase;

  const base::Time expiry = base::Time::Now();
  base::Time next_expiry = base::Time::Max();

  for (auto& item : categories_) {
    Category category = item.first;
    CategoryContent* content = &item.second;

    // Move expired snippets over into |to_delete|.
    NTPSnippet::PtrVector to_delete;
    for (std::unique_ptr<NTPSnippet>& snippet : content->snippets) {
      if (snippet->expiry_date() <= expiry)
        to_delete.emplace_back(std::move(snippet));
    }
    Compact(&content->snippets);

    // Move expired dismissed snippets over into |to_delete| as well.
    for (std::unique_ptr<NTPSnippet>& snippet : content->dismissed) {
      if (snippet->expiry_date() <= expiry)
        to_delete.emplace_back(std::move(snippet));
    }
    Compact(&content->dismissed);

    // Finally, actually delete the removed snippets from the DB.
    if (category == articles_category_)
      database_->DeleteSnippets(to_delete);

    if (content->snippets.empty() && content->dismissed.empty()) {
      if ((category != articles_category_) && !content->provided_by_server)
        categories_to_erase.push_back(category);
      continue;
    }

    for (const auto& snippet : content->snippets) {
      if (snippet->expiry_date() < next_expiry)
        next_expiry = snippet->expiry_date();
    }
    for (const auto& snippet : content->dismissed) {
      if (snippet->expiry_date() < next_expiry)
        next_expiry = snippet->expiry_date();
    }
  }

  for (Category category : categories_to_erase) {
    UpdateCategoryStatus(category, CategoryStatus::NOT_PROVIDED);
    categories_.erase(category);
  }

  // Unless there are no snippets left, schedule a timer for the next expiry.
  DCHECK_GT(next_expiry, expiry);
  if (next_expiry < base::Time::Max()) {
    expiry_timer_.Start(FROM_HERE, next_expiry - expiry,
                        base::Bind(&NTPSnippetsService::ClearExpiredSnippets,
                                   base::Unretained(this)));
  }
}

void NTPSnippetsService::NukeAllSnippets() {
  std::vector<Category> categories_to_erase;

  // Empty the ARTICLES category and remove all others, since they may or may
  // not be personalized.
  for (const auto& item : categories_) {
    Category category = item.first;

    ClearCachedSuggestions(category);
    ClearDismissedSuggestionsForDebugging(category);

    if (category == articles_category_) {
      // Temporarily enter an "explicitly disabled" state, so that any open UIs
      // will clear the suggestions too.
      CategoryContent& content = categories_[category];
      if (content.status != CategoryStatus::CATEGORY_EXPLICITLY_DISABLED) {
        CategoryStatus old_category_status = content.status;
        UpdateCategoryStatus(category,
                             CategoryStatus::CATEGORY_EXPLICITLY_DISABLED);
        UpdateCategoryStatus(category, old_category_status);
      }
    } else {
      // Remove other categories entirely; they may or may not reappear.
      UpdateCategoryStatus(category, CategoryStatus::NOT_PROVIDED);
      categories_to_erase.push_back(category);
    }
  }

  for (Category category : categories_to_erase) {
    categories_.erase(category);
  }
}

void NTPSnippetsService::OnSnippetImageFetchedFromDatabase(
    const ImageFetchedCallback& callback,
    const std::string& suggestion_id,
    std::string data) {
  // |image_decoder_| is null in tests.
  if (image_decoder_ && !data.empty()) {
    image_decoder_->DecodeImage(
        std::move(data),
        base::Bind(&NTPSnippetsService::OnSnippetImageDecodedFromDatabase,
                   base::Unretained(this), callback, suggestion_id));
    return;
  }

  // Fetching from the DB failed; start a network fetch.
  FetchSnippetImageFromNetwork(suggestion_id, callback);
}

void NTPSnippetsService::OnSnippetImageDecodedFromDatabase(
    const ImageFetchedCallback& callback,
    const std::string& suggestion_id,
    const gfx::Image& image) {
  if (!image.IsEmpty()) {
    callback.Run(image);
    return;
  }

  // If decoding the image failed, delete the DB entry.
  std::string snippet_id = GetWithinCategoryIDFromUniqueID(suggestion_id);
  database_->DeleteImage(snippet_id);

  FetchSnippetImageFromNetwork(suggestion_id, callback);
}

void NTPSnippetsService::FetchSnippetImageFromNetwork(
    const std::string& suggestion_id,
    const ImageFetchedCallback& callback) {
  Category category = GetCategoryFromUniqueID(suggestion_id);
  std::string snippet_id = GetWithinCategoryIDFromUniqueID(suggestion_id);

  auto category_it = categories_.find(category);
  if (category_it == categories_.end()) {
    OnSnippetImageDecodedFromNetwork(callback, suggestion_id, gfx::Image());
    return;
  }

  const CategoryContent& content = category_it->second;
  auto it =
      std::find_if(content.snippets.begin(), content.snippets.end(),
                   [&snippet_id](const std::unique_ptr<NTPSnippet>& snippet) {
                     return snippet->id() == snippet_id;
                   });

  if (it == content.snippets.end() ||
      !thumbnail_requests_throttler_.DemandQuotaForRequest(
          /*interactive_request=*/true)) {
    // Return an empty image. Directly, this is never synchronous with the
    // original FetchSuggestionImage() call - an asynchronous database query has
    // happened in the meantime.
    OnSnippetImageDecodedFromNetwork(callback, suggestion_id, gfx::Image());
    return;
  }

  const NTPSnippet& snippet = *it->get();

  // TODO(jkrcal): We probably should rename OnImageDataFetched() to
  // CacheImageData(). This would document that this is actually independent
  // from the individual fetch-flow.
  // The image fetcher calls OnImageDataFetched() with the raw data (this object
  // is an ImageFetcherDelegate) and then also
  // OnSnippetImageDecodedFromNetwork() after the raw data gets decoded.
  image_fetcher_->StartOrQueueNetworkRequest(
      suggestion_id, snippet.salient_image_url(),
      base::Bind(&NTPSnippetsService::OnSnippetImageDecodedFromNetwork,
                 base::Unretained(this), callback));
}

void NTPSnippetsService::OnSnippetImageDecodedFromNetwork(
    const ImageFetchedCallback& callback,
    const std::string& suggestion_id,
    const gfx::Image& image) {
  callback.Run(image);
}

void NTPSnippetsService::EnterStateEnabled(bool fetch_snippets) {
  if (fetch_snippets)
    FetchSnippets(/*force_request=*/false);

  // FetchSnippets should set the status to |AVAILABLE_LOADING| if relevant,
  // otherwise we transition to |AVAILABLE| here.
  if (categories_[articles_category_].status !=
      CategoryStatus::AVAILABLE_LOADING) {
    UpdateCategoryStatus(articles_category_, CategoryStatus::AVAILABLE);
  }

  // If host restrictions are enabled, register for host list updates.
  // |suggestions_service_| can be null in tests.
  if (snippets_fetcher_->UsesHostRestrictions() && suggestions_service_) {
    suggestions_service_subscription_ =
        suggestions_service_->AddCallback(base::Bind(
            &NTPSnippetsService::OnSuggestionsChanged, base::Unretained(this)));
  }

  RescheduleFetching();
}

void NTPSnippetsService::EnterStateDisabled() {
  std::vector<Category> categories_to_erase;

  // Empty the ARTICLES category and remove all others, since they may or may
  // not be personalized.
  for (const auto& item : categories_) {
    Category category = item.first;
    ClearCachedSuggestions(category);
    ClearDismissedSuggestionsForDebugging(category);
    if (category != articles_category_) {
      UpdateCategoryStatus(category, CategoryStatus::NOT_PROVIDED);
      categories_to_erase.push_back(category);
    }
  }

  for (Category category : categories_to_erase) {
    categories_.erase(category);
  }

  expiry_timer_.Stop();
  suggestions_service_subscription_.reset();
  RescheduleFetching();
}

void NTPSnippetsService::EnterStateError() {
  expiry_timer_.Stop();
  suggestions_service_subscription_.reset();
  RescheduleFetching();
  snippets_status_service_.reset();
}

void NTPSnippetsService::FinishInitialization() {
  if (nuke_after_load_) {
    NukeAllSnippets();
    nuke_after_load_ = false;
  }

  snippets_fetcher_->SetCallback(
      base::Bind(&NTPSnippetsService::OnFetchFinished, base::Unretained(this)));

  // |image_fetcher_| can be null in tests.
  if (image_fetcher_) {
    image_fetcher_->SetImageFetcherDelegate(this);
    image_fetcher_->SetDataUseServiceName(
        data_use_measurement::DataUseUserData::NTP_SNIPPETS);
  }

  // Note: Initializing the status service will run the callback right away with
  // the current state.
  snippets_status_service_->Init(base::Bind(
      &NTPSnippetsService::OnDisabledReasonChanged, base::Unretained(this)));

  // Always notify here even if we got nothing from the database, because we
  // don't know how long the fetch will take or if it will even complete.
  NotifyNewSuggestions();
}

void NTPSnippetsService::OnDisabledReasonChanged(
    DisabledReason disabled_reason) {
  switch (disabled_reason) {
    case DisabledReason::NONE:
      // Do not change the status. That will be done in EnterStateEnabled()
      EnterState(State::READY);
      break;

    case DisabledReason::EXPLICITLY_DISABLED:
      EnterState(State::DISABLED);
      UpdateAllCategoryStatus(CategoryStatus::CATEGORY_EXPLICITLY_DISABLED);
      break;

    case DisabledReason::SIGNED_OUT:
      EnterState(State::DISABLED);
      UpdateAllCategoryStatus(CategoryStatus::SIGNED_OUT);
      break;
  }
}

void NTPSnippetsService::EnterState(State state) {
  if (state == state_)
    return;

  switch (state) {
    case State::NOT_INITED:
      // Initial state, it should not be possible to get back there.
      NOTREACHED();
      return;

    case State::READY: {
      DCHECK(state_ == State::NOT_INITED || state_ == State::DISABLED);

      bool fetch_snippets =
          categories_[articles_category_].snippets.empty() || fetch_after_load_;
      DVLOG(1) << "Entering state: READY";
      state_ = State::READY;
      fetch_after_load_ = false;
      EnterStateEnabled(fetch_snippets);
      return;
    }

    case State::DISABLED:
      DCHECK(state_ == State::NOT_INITED || state_ == State::READY);

      DVLOG(1) << "Entering state: DISABLED";
      state_ = State::DISABLED;
      EnterStateDisabled();
      return;

    case State::ERROR_OCCURRED:
      DVLOG(1) << "Entering state: ERROR_OCCURRED";
      state_ = State::ERROR_OCCURRED;
      EnterStateError();
      return;
  }
}

void NTPSnippetsService::NotifyNewSuggestions() {
  for (const auto& item : categories_) {
    Category category = item.first;
    const CategoryContent& content = item.second;

    std::vector<ContentSuggestion> result;
    for (const std::unique_ptr<NTPSnippet>& snippet : content.snippets) {
      // TODO(sfiera): if a snippet is not going to be displayed, move it
      // directly to content.dismissed on fetch. Otherwise, we might prune
      // other snippets to get down to kMaxSnippetCount, only to hide one of the
      // incomplete ones we kept.
      if (!snippet->is_complete())
        continue;
      ContentSuggestion suggestion(MakeUniqueID(category, snippet->id()),
                                   snippet->best_source().url);
      suggestion.set_amp_url(snippet->best_source().amp_url);
      suggestion.set_title(base::UTF8ToUTF16(snippet->title()));
      suggestion.set_snippet_text(base::UTF8ToUTF16(snippet->snippet()));
      suggestion.set_publish_date(snippet->publish_date());
      suggestion.set_publisher_name(
          base::UTF8ToUTF16(snippet->best_source().publisher_name));
      suggestion.set_score(snippet->score());
      result.emplace_back(std::move(suggestion));
    }

    DVLOG(1) << "NotifyNewSuggestions(): " << result.size()
             << " items in category " << category;
    observer()->OnNewSuggestions(this, category, std::move(result));
  }
}

void NTPSnippetsService::UpdateCategoryStatus(Category category,
                                              CategoryStatus status) {
  DCHECK(categories_.find(category) != categories_.end());
  CategoryContent& content = categories_[category];
  if (status == content.status)
    return;

  DVLOG(1) << "UpdateCategoryStatus(): " << category.id() << ": "
           << static_cast<int>(content.status) << " -> "
           << static_cast<int>(status);
  content.status = status;
  observer()->OnCategoryStatusChanged(this, category, content.status);
}

void NTPSnippetsService::UpdateAllCategoryStatus(CategoryStatus status) {
  for (const auto& category : categories_) {
    UpdateCategoryStatus(category.first, status);
  }
}

NTPSnippetsService::CategoryContent::CategoryContent() = default;
NTPSnippetsService::CategoryContent::CategoryContent(CategoryContent&&) =
    default;
NTPSnippetsService::CategoryContent::~CategoryContent() = default;
NTPSnippetsService::CategoryContent& NTPSnippetsService::CategoryContent::
operator=(CategoryContent&&) = default;

}  // namespace ntp_snippets

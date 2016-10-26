// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/ntp_snippets_service.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/history/core/browser/history_service.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/ntp_snippets_database.h"
#include "components/ntp_snippets/switches.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

namespace {

// Number of snippets requested to the server. Consider replacing sparse UMA
// histograms with COUNTS() if this number increases beyond 50.
const int kMaxSnippetCount = 10;

// Number of archived snippets we keep around in memory.
const int kMaxArchivedSnippetCount = 200;

// Default values for fetching intervals, fallback and wifi.
const double kDefaultFetchingIntervalRareNtpUser[] = {48.0, 24.0};
const double kDefaultFetchingIntervalActiveNtpUser[] = {24.0, 6.0};
const double kDefaultFetchingIntervalActiveSuggestionsConsumer[] = {24.0, 6.0};

// Variation parameters than can override the default fetching intervals.
const char* kFetchingIntervalParamNameRareNtpUser[] = {
    "fetching_interval_hours-fallback-rare_ntp_user",
    "fetching_interval_hours-wifi-rare_ntp_user"};
const char* kFetchingIntervalParamNameActiveNtpUser[] = {
    "fetching_interval_hours-fallback-active_ntp_user",
    "fetching_interval_hours-wifi-active_ntp_user"};
const char* kFetchingIntervalParamNameActiveSuggestionsConsumer[] = {
    "fetching_interval_hours-fallback-active_suggestions_consumer",
    "fetching_interval_hours-wifi-active_suggestions_consumer"};

const int kDefaultExpiryTimeMins = 3 * 24 * 60;

// Keys for storing CategoryContent info in prefs.
const char kCategoryContentId[] = "id";
const char kCategoryContentTitle[] = "title";
const char kCategoryContentProvidedByServer[] = "provided_by_server";

base::TimeDelta GetFetchingInterval(bool is_wifi,
                                    UserClassifier::UserClass user_class) {
  double value_hours = 0.0;

  const int index = is_wifi ? 1 : 0;
  const char* param_name = "";
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      value_hours = kDefaultFetchingIntervalRareNtpUser[index];
      param_name = kFetchingIntervalParamNameRareNtpUser[index];
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      value_hours = kDefaultFetchingIntervalActiveNtpUser[index];
      param_name = kFetchingIntervalParamNameActiveNtpUser[index];
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      value_hours = kDefaultFetchingIntervalActiveSuggestionsConsumer[index];
      param_name = kFetchingIntervalParamNameActiveSuggestionsConsumer[index];
      break;
  }

  // The default value can be overridden by a variation parameter.
  std::string param_value_str = variations::GetVariationParamValueByFeature(
        ntp_snippets::kArticleSuggestionsFeature, param_name);
  if (!param_value_str.empty()) {
    double param_value_hours = 0.0;
    if (base::StringToDouble(param_value_str, &param_value_hours))
      value_hours = param_value_hours;
    else
      LOG(WARNING) << "Invalid value for variation parameter " << param_name;
  }

  return base::TimeDelta::FromSecondsD(value_hours * 3600.0);
}

std::unique_ptr<std::vector<std::string>> GetSnippetIDVector(
    const NTPSnippet::PtrVector& snippets) {
  auto result = base::MakeUnique<std::vector<std::string>>();
  for (const auto& snippet : snippets) {
    result->push_back(snippet->id());
  }
  return result;
}

bool HasIntersection(const std::vector<std::string>& a,
                     const std::set<std::string>& b) {
  for (const std::string& item : a) {
    if (base::ContainsValue(b, item)) return true;
  }
  return false;
}

void EraseByPrimaryID(NTPSnippet::PtrVector* snippets,
                      const std::vector<std::string>& ids) {
  std::set<std::string> ids_lookup(ids.begin(), ids.end());
  snippets->erase(
      std::remove_if(snippets->begin(), snippets->end(),
                     [&ids_lookup](const std::unique_ptr<NTPSnippet>& snippet) {
                       return base::ContainsValue(ids_lookup, snippet->id());
                     }),
      snippets->end());
}

void EraseMatchingSnippets(NTPSnippet::PtrVector* snippets,
                           const NTPSnippet::PtrVector& compare_against) {
  std::set<std::string> compare_against_ids;
  for (const std::unique_ptr<NTPSnippet>& snippet : compare_against) {
    const std::vector<std::string>& snippet_ids = snippet->GetAllIDs();
    compare_against_ids.insert(snippet_ids.begin(), snippet_ids.end());
  }
  snippets->erase(
      std::remove_if(
          snippets->begin(), snippets->end(),
          [&compare_against_ids](const std::unique_ptr<NTPSnippet>& snippet) {
            return HasIntersection(snippet->GetAllIDs(), compare_against_ids);
          }),
      snippets->end());
}

void RemoveNullPointers(NTPSnippet::PtrVector* snippets) {
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
    const std::string& application_language_code,
    const UserClassifier* user_classifier,
    NTPSnippetsScheduler* scheduler,
    std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher,
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    std::unique_ptr<NTPSnippetsDatabase> database,
    std::unique_ptr<NTPSnippetsStatusService> status_service)
    : ContentSuggestionsProvider(observer, category_factory),
      state_(State::NOT_INITED),
      pref_service_(pref_service),
      articles_category_(
          category_factory->FromKnownCategory(KnownCategories::ARTICLES)),
      application_language_code_(application_language_code),
      user_classifier_(user_classifier),
      scheduler_(scheduler),
      snippets_fetcher_(std::move(snippets_fetcher)),
      image_fetcher_(std::move(image_fetcher)),
      image_decoder_(std::move(image_decoder)),
      database_(std::move(database)),
      snippets_status_service_(std::move(status_service)),
      fetch_when_ready_(false),
      nuke_when_initialized_(false),
      thumbnail_requests_throttler_(
          pref_service,
          RequestThrottler::RequestType::CONTENT_SUGGESTION_THUMBNAIL) {
  RestoreCategoriesFromPrefs();
  // The articles category always exists. Add it if we didn't get it from prefs.
  // TODO(treib): Rethink this.
  if (!base::ContainsKey(categories_, articles_category_)) {
    categories_[articles_category_] = CategoryContent();
    categories_[articles_category_].localized_title =
        l10n_util::GetStringUTF16(IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_HEADER);
  }
  // Tell the observer about all the categories.
  for (const auto& entry : categories_) {
    observer->OnCategoryStatusChanged(this, entry.first, entry.second.status);
  }

  if (database_->IsErrorState()) {
    EnterState(State::ERROR_OCCURRED);
    UpdateAllCategoryStatus(CategoryStatus::LOADING_ERROR);
    return;
  }

  database_->SetErrorCallback(base::Bind(&NTPSnippetsService::OnDatabaseError,
                                         base::Unretained(this)));

  // We transition to other states while finalizing the initialization, when the
  // database is done loading.
  database_load_start_ = base::TimeTicks::Now();
  database_->LoadSnippets(base::Bind(&NTPSnippetsService::OnDatabaseLoaded,
                                     base::Unretained(this)));
}

NTPSnippetsService::~NTPSnippetsService() = default;

// static
void NTPSnippetsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // TODO(treib): Add cleanup logic for prefs::kSnippetHosts, then remove it
  // completely after M56.
  registry->RegisterListPref(prefs::kSnippetHosts);
  registry->RegisterListPref(prefs::kRemoteSuggestionCategories);
  registry->RegisterInt64Pref(prefs::kSnippetBackgroundFetchingIntervalWifi, 0);
  registry->RegisterInt64Pref(prefs::kSnippetBackgroundFetchingIntervalFallback,
                              0);

  NTPSnippetsStatusService::RegisterProfilePrefs(registry);
}

void NTPSnippetsService::FetchSnippets(bool interactive_request) {
  if (ready())
    FetchSnippetsFromHosts(std::set<std::string>(), interactive_request);
  else
    fetch_when_ready_ = true;
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

void NTPSnippetsService::RescheduleFetching(bool force) {
  // The scheduler only exists on Android so far, it's null on other platforms.
  if (!scheduler_)
    return;

  if (ready()) {
    base::TimeDelta old_interval_wifi =
        base::TimeDelta::FromInternalValue(pref_service_->GetInt64(
            prefs::kSnippetBackgroundFetchingIntervalWifi));
    base::TimeDelta old_interval_fallback =
        base::TimeDelta::FromInternalValue(pref_service_->GetInt64(
            prefs::kSnippetBackgroundFetchingIntervalFallback));
    UserClassifier::UserClass user_class = user_classifier_->GetUserClass();
    base::TimeDelta interval_wifi =
        GetFetchingInterval(/*is_wifi=*/true, user_class);
    base::TimeDelta interval_fallback =
        GetFetchingInterval(/*is_wifi=*/false, user_class);
    if (force || interval_wifi != old_interval_wifi ||
        interval_fallback != old_interval_fallback) {
      scheduler_->Schedule(interval_wifi, interval_fallback);
      pref_service_->SetInt64(prefs::kSnippetBackgroundFetchingIntervalWifi,
                              interval_wifi.ToInternalValue());
      pref_service_->SetInt64(
          prefs::kSnippetBackgroundFetchingIntervalFallback,
          interval_fallback.ToInternalValue());
    }
  } else {
    // If we're NOT_INITED, we don't know whether to schedule or unschedule.
    // If |force| is false, all is well: We'll reschedule on the next state
    // change anyway. If it's true, then unschedule here, to make sure that the
    // next reschedule actually happens.
    if (state_ != State::NOT_INITED || force) {
      scheduler_->Unschedule();
      pref_service_->ClearPref(prefs::kSnippetBackgroundFetchingIntervalWifi);
      pref_service_->ClearPref(
          prefs::kSnippetBackgroundFetchingIntervalFallback);
    }
  }
}

CategoryStatus NTPSnippetsService::GetCategoryStatus(Category category) {
  DCHECK(base::ContainsKey(categories_, category));
  return categories_[category].status;
}

CategoryInfo NTPSnippetsService::GetCategoryInfo(Category category) {
  DCHECK(base::ContainsKey(categories_, category));
  const CategoryContent& content = categories_[category];
  return CategoryInfo(
      content.localized_title, ContentSuggestionsCardLayout::FULL_CARD,
      /*has_more_button=*/false,
      /*show_if_empty=*/true,
      l10n_util::GetStringUTF16(IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_EMPTY));
}

void NTPSnippetsService::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  if (!ready())
    return;

  DCHECK(base::ContainsKey(categories_, suggestion_id.category()));

  CategoryContent* content = &categories_[suggestion_id.category()];
  auto it = std::find_if(
      content->snippets.begin(), content->snippets.end(),
      [&suggestion_id](const std::unique_ptr<NTPSnippet>& snippet) {
        return snippet->id() == suggestion_id.id_within_category();
      });
  if (it == content->snippets.end())
    return;

  (*it)->set_dismissed(true);

  database_->SaveSnippet(**it);
  database_->DeleteImage(suggestion_id.id_within_category());

  content->dismissed.push_back(std::move(*it));
  content->snippets.erase(it);
}

void NTPSnippetsService::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  database_->LoadImage(
      suggestion_id.id_within_category(),
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
    nuke_when_initialized_ = true;
  else
    NukeAllSnippets();
}

void NTPSnippetsService::ClearCachedSuggestions(Category category) {
  if (!initialized())
    return;

  if (!base::ContainsKey(categories_, category))
    return;
  CategoryContent* content = &categories_[category];
  if (content->snippets.empty())
    return;

  database_->DeleteSnippets(GetSnippetIDVector(content->snippets));
  database_->DeleteImages(GetSnippetIDVector(content->snippets));
  content->snippets.clear();

  NotifyNewSuggestions(category);
}

void NTPSnippetsService::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  DCHECK(base::ContainsKey(categories_, category));

  std::vector<ContentSuggestion> result;
  const CategoryContent& content = categories_[category];
  for (const std::unique_ptr<NTPSnippet>& snippet : content.dismissed) {
    if (!snippet->is_complete())
      continue;
    ContentSuggestion suggestion(category, snippet->id(),
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
  DCHECK(base::ContainsKey(categories_, category));

  if (!initialized())
    return;

  CategoryContent* content = &categories_[category];
  if (content->dismissed.empty())
    return;

  database_->DeleteSnippets(GetSnippetIDVector(content->dismissed));
  // The image got already deleted when the suggestion was dismissed.

  content->dismissed.clear();
}

// static
int NTPSnippetsService::GetMaxSnippetCountForTesting() {
  return kMaxSnippetCount;
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

GURL NTPSnippetsService::FindSnippetImageUrl(
    const ContentSuggestion::ID& suggestion_id) const {
  DCHECK(base::ContainsKey(categories_, suggestion_id.category()));

  const CategoryContent& content = categories_.at(suggestion_id.category());
  const NTPSnippet* snippet =
      content.FindSnippet(suggestion_id.id_within_category());
  if (!snippet)
    return GURL();
  return snippet->salient_image_url();
}

// image_fetcher::ImageFetcherDelegate implementation.
void NTPSnippetsService::OnImageDataFetched(
    const std::string& id_within_category,
    const std::string& image_data) {
  if (image_data.empty())
    return;

  // Only save the image if the corresponding snippet still exists.
  bool found = false;
  for (const std::pair<const Category, CategoryContent>& entry : categories_) {
    if (entry.second.FindSnippet(id_within_category)) {
      found = true;
      break;
    }
  }
  if (!found)
    return;

  // Only cache the data in the DB, the actual serving is done in the callback
  // provided to |image_fetcher_| (OnSnippetImageDecodedFromNetwork()).
  database_->SaveImage(id_within_category, image_data);
}

void NTPSnippetsService::OnDatabaseLoaded(NTPSnippet::PtrVector snippets) {
  if (state_ == State::ERROR_OCCURRED)
    return;
  DCHECK(state_ == State::NOT_INITED);
  DCHECK(base::ContainsKey(categories_, articles_category_));

  base::TimeDelta database_load_time =
      base::TimeTicks::Now() - database_load_start_;
  UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.Snippets.DatabaseLoadTime",
                             database_load_time);

  NTPSnippet::PtrVector to_delete;
  for (std::unique_ptr<NTPSnippet>& snippet : snippets) {
    Category snippet_category =
        category_factory()->FromRemoteCategory(snippet->remote_category_id());
    // We should already know about the category.
    if (!base::ContainsKey(categories_, snippet_category)) {
      DLOG(WARNING) << "Loaded a suggestion for unknown category "
                    << snippet_category << " from the DB; deleting";
      to_delete.emplace_back(std::move(snippet));
      continue;
    }

    CategoryContent* content = &categories_[snippet_category];
    if (snippet->is_dismissed())
      content->dismissed.emplace_back(std::move(snippet));
    else
      content->snippets.emplace_back(std::move(snippet));
  }
  if (!to_delete.empty()) {
    database_->DeleteSnippets(GetSnippetIDVector(to_delete));
    database_->DeleteImages(GetSnippetIDVector(to_delete));
  }

  // Sort the suggestions in each category.
  // TODO(treib): Persist the actual order in the DB somehow? crbug.com/654409
  for (auto& entry : categories_) {
    CategoryContent* content = &entry.second;
    std::sort(content->snippets.begin(), content->snippets.end(),
              [](const std::unique_ptr<NTPSnippet>& lhs,
                 const std::unique_ptr<NTPSnippet>& rhs) {
                return lhs->score() > rhs->score();
              });
  }

  // TODO(tschumann): If I move ClearExpiredDismisedSnippets() to the beginning
  // of the function, it essentially does nothing but tests are still green. Fix
  // this!
  ClearExpiredDismissedSnippets();
  ClearOrphanedImages();
  FinishInitialization();
}

void NTPSnippetsService::OnDatabaseError() {
  EnterState(State::ERROR_OCCURRED);
  UpdateAllCategoryStatus(CategoryStatus::LOADING_ERROR);
}

void NTPSnippetsService::OnFetchFinished(
    NTPSnippetsFetcher::OptionalFetchedCategories fetched_categories) {
  if (!ready())
    return;

  // Mark all categories as not provided by the server in the latest fetch. The
  // ones we got will be marked again below.
  for (auto& item : categories_) {
    CategoryContent* content = &item.second;
    content->provided_by_server = false;
  }

  // Clear up expired dismissed snippets before we use them to filter new ones.
  ClearExpiredDismissedSnippets();

  // If snippets were fetched successfully, update our |categories_| from each
  // category provided by the server.
  if (fetched_categories) {
    // TODO(treib): Reorder |categories_| to match the order we received from
    // the server. crbug.com/653816
    // TODO(jkrcal): A bit hard to understand with so many variables called
    // "*categor*". Isn't here some room for simplification?
    for (NTPSnippetsFetcher::FetchedCategory& fetched_category :
         *fetched_categories) {
      Category category = fetched_category.category;

      // The ChromeReader backend doesn't provide category titles, so don't
      // overwrite the existing title for ARTICLES if the new one is empty.
      // TODO(treib): Remove this check after we fully switch to the content
      // suggestions backend.
      if (category != articles_category_ ||
          !fetched_category.localized_title.empty()) {
        categories_[category].localized_title =
            fetched_category.localized_title;
      }
      categories_[category].provided_by_server = true;

      // TODO(tschumann): Remove this histogram once we only talk to the content
      // suggestions cloud backend.
      if (category == articles_category_) {
        UMA_HISTOGRAM_SPARSE_SLOWLY(
            "NewTabPage.Snippets.NumArticlesFetched",
            std::min(fetched_category.snippets.size(),
                     static_cast<size_t>(kMaxSnippetCount + 1)));
      }
      ReplaceSnippets(category, std::move(fetched_category.snippets));
    }
  }

  // We might have gotten new categories (or updated the titles of existing
  // ones), so update the pref.
  StoreCategoriesToPrefs();

  for (const auto& item : categories_) {
    Category category = item.first;
    UpdateCategoryStatus(category, CategoryStatus::AVAILABLE);
    // TODO(sfiera): notify only when a category changed above.
    NotifyNewSuggestions(category);
  }

  // TODO(sfiera): equivalent metrics for non-articles.
  const CategoryContent& content = categories_[articles_category_];
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumArticles",
                              content.snippets.size());
  if (content.snippets.empty() && !content.dismissed.empty()) {
    UMA_HISTOGRAM_COUNTS("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded",
                         content.dismissed.size());
  }

  // Reschedule after a successful fetch. This resets all currently scheduled
  // fetches, to make sure the fallback interval triggers only if no wifi fetch
  // succeeded, and also that we don't do a background fetch immediately after
  // a user-initiated one.
  if (fetched_categories)
    RescheduleFetching(true);
}

void NTPSnippetsService::ArchiveSnippets(Category category,
                                         NTPSnippet::PtrVector* to_archive) {
  CategoryContent* content = &categories_[category];
  database_->DeleteSnippets(GetSnippetIDVector(*to_archive));
  // Do not delete the thumbnail images as they are still handy on open NTPs.

  // Archive previous snippets - move them at the beginning of the list.
  content->archived.insert(content->archived.begin(),
                           std::make_move_iterator(to_archive->begin()),
                           std::make_move_iterator(to_archive->end()));
  RemoveNullPointers(to_archive);

  // If there are more archived snippets than we want to keep, delete the
  // oldest ones by their fetch time (which are always in the back).
  if (content->archived.size() > kMaxArchivedSnippetCount) {
    NTPSnippet::PtrVector to_delete(
        std::make_move_iterator(content->archived.begin() +
                                kMaxArchivedSnippetCount),
        std::make_move_iterator(content->archived.end()));
    content->archived.resize(kMaxArchivedSnippetCount);
    database_->DeleteImages(GetSnippetIDVector(to_delete));
  }
}

void NTPSnippetsService::ReplaceSnippets(Category category,
                                         NTPSnippet::PtrVector new_snippets) {
  DCHECK(ready());
  CategoryContent* content = &categories_[category];

  // Remove new snippets that have been dismissed.
  EraseMatchingSnippets(&new_snippets, content->dismissed);

  // Fill in default publish/expiry dates where required.
  for (std::unique_ptr<NTPSnippet>& snippet : new_snippets) {
    if (snippet->publish_date().is_null())
      snippet->set_publish_date(base::Time::Now());
    if (snippet->expiry_date().is_null()) {
      snippet->set_expiry_date(
          snippet->publish_date() +
          base::TimeDelta::FromMinutes(kDefaultExpiryTimeMins));
    }
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

  // Do not touch the current set of snippets if the newly fetched one is empty.
  if (new_snippets.empty())
    return;

  // It's entirely possible that the newly fetched snippets contain articles
  // that have been present before.
  // Since archival removes snippets from the database (indexed by
  // snippet->id()), we need to make sure to only archive snippets that don't
  // appear with the same ID in the new suggestions (it's fine for additional
  // IDs though).
  EraseByPrimaryID(&content->snippets, *GetSnippetIDVector(new_snippets));
  ArchiveSnippets(category, &content->snippets);

  // Save new articles to the DB.
  database_->SaveSnippets(new_snippets);

  content->snippets = std::move(new_snippets);
}

void NTPSnippetsService::ClearExpiredDismissedSnippets() {
  std::vector<Category> categories_to_erase;

  const base::Time now = base::Time::Now();

  for (auto& item : categories_) {
    Category category = item.first;
    CategoryContent* content = &item.second;

    NTPSnippet::PtrVector to_delete;
    // Move expired dismissed snippets over into |to_delete|.
    for (std::unique_ptr<NTPSnippet>& snippet : content->dismissed) {
      if (snippet->expiry_date() <= now)
        to_delete.emplace_back(std::move(snippet));
    }
    RemoveNullPointers(&content->dismissed);

    // Delete the removed article suggestions from the DB.
    database_->DeleteSnippets(GetSnippetIDVector(to_delete));
    // The image got already deleted when the suggestion was dismissed.

    if (content->snippets.empty() && content->dismissed.empty() &&
        category != articles_category_ && !content->provided_by_server) {
      categories_to_erase.push_back(category);
    }
  }

  for (Category category : categories_to_erase) {
    UpdateCategoryStatus(category, CategoryStatus::NOT_PROVIDED);
    categories_.erase(category);
  }

  StoreCategoriesToPrefs();
}

void NTPSnippetsService::ClearOrphanedImages() {
  auto alive_snippets = base::MakeUnique<std::set<std::string>>();
  for (const auto& entry : categories_) {
    const CategoryContent& content = entry.second;
    for (const auto& snippet_ptr : content.snippets) {
      alive_snippets->insert(snippet_ptr->id());
    }
    for (const auto& snippet_ptr : content.dismissed) {
      alive_snippets->insert(snippet_ptr->id());
    }
  }
  database_->GarbageCollectImages(std::move(alive_snippets));
}

void NTPSnippetsService::NukeAllSnippets() {
  std::vector<Category> categories_to_erase;

  // Empty the ARTICLES category and remove all others, since they may or may
  // not be personalized.
  for (const auto& item : categories_) {
    Category category = item.first;

    ClearCachedSuggestions(category);
    ClearDismissedSuggestionsForDebugging(category);

    UpdateCategoryStatus(category, CategoryStatus::NOT_PROVIDED);

    // Remove the category entirely; it may or may not reappear.
    if (category != articles_category_)
      categories_to_erase.push_back(category);
  }

  for (Category category : categories_to_erase) {
    categories_.erase(category);
  }

  StoreCategoriesToPrefs();
}

void NTPSnippetsService::OnSnippetImageFetchedFromDatabase(
    const ImageFetchedCallback& callback,
    const ContentSuggestion::ID& suggestion_id,
    std::string data) {
  // |image_decoder_| is null in tests.
  if (image_decoder_ && !data.empty()) {
    image_decoder_->DecodeImage(
        data, base::Bind(&NTPSnippetsService::OnSnippetImageDecodedFromDatabase,
                         base::Unretained(this), callback, suggestion_id));
    return;
  }

  // Fetching from the DB failed; start a network fetch.
  FetchSnippetImageFromNetwork(suggestion_id, callback);
}

void NTPSnippetsService::OnSnippetImageDecodedFromDatabase(
    const ImageFetchedCallback& callback,
    const ContentSuggestion::ID& suggestion_id,
    const gfx::Image& image) {
  if (!image.IsEmpty()) {
    callback.Run(image);
    return;
  }

  // If decoding the image failed, delete the DB entry.
  database_->DeleteImage(suggestion_id.id_within_category());

  FetchSnippetImageFromNetwork(suggestion_id, callback);
}

void NTPSnippetsService::FetchSnippetImageFromNetwork(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  if (!base::ContainsKey(categories_, suggestion_id.category())) {
    OnSnippetImageDecodedFromNetwork(
        callback, suggestion_id.id_within_category(), gfx::Image());
    return;
  }

  GURL image_url = FindSnippetImageUrl(suggestion_id);

  if (image_url.is_empty() ||
      !thumbnail_requests_throttler_.DemandQuotaForRequest(
          /*interactive_request=*/true)) {
    // Return an empty image. Directly, this is never synchronous with the
    // original FetchSuggestionImage() call - an asynchronous database query has
    // happened in the meantime.
    OnSnippetImageDecodedFromNetwork(
        callback, suggestion_id.id_within_category(), gfx::Image());
    return;
  }

  image_fetcher_->StartOrQueueNetworkRequest(
      suggestion_id.id_within_category(), image_url,
      base::Bind(&NTPSnippetsService::OnSnippetImageDecodedFromNetwork,
                 base::Unretained(this), callback));
}

void NTPSnippetsService::OnSnippetImageDecodedFromNetwork(
    const ImageFetchedCallback& callback,
    const std::string& id_within_category,
    const gfx::Image& image) {
  callback.Run(image);
}

void NTPSnippetsService::EnterStateReady() {
  if (nuke_when_initialized_) {
    NukeAllSnippets();
    nuke_when_initialized_ = false;
  }

  if (categories_[articles_category_].snippets.empty() || fetch_when_ready_) {
    // TODO(jkrcal): Fetching snippets automatically upon creation of this
    // lazily created service can cause troubles, e.g. in unit tests where
    // network I/O is not allowed.
    // Either add a DCHECK here that we actually are allowed to do network I/O
    // or change the logic so that some explicit call is always needed for the
    // network request.
    FetchSnippets(/*interactive_request=*/false);
    fetch_when_ready_ = false;
  }

  for (const auto& item : categories_) {
    Category category = item.first;
    const CategoryContent& content = item.second;
    // FetchSnippets has set the status to |AVAILABLE_LOADING| if relevant,
    // otherwise we transition to |AVAILABLE| here.
    if (content.status != CategoryStatus::AVAILABLE_LOADING)
      UpdateCategoryStatus(category, CategoryStatus::AVAILABLE);
  }
}

void NTPSnippetsService::EnterStateDisabled() {
  NukeAllSnippets();
}

void NTPSnippetsService::EnterStateError() {
  snippets_status_service_.reset();
}

void NTPSnippetsService::FinishInitialization() {
  if (nuke_when_initialized_) {
    // We nuke here in addition to EnterStateReady, so that it happens even if
    // we enter the DISABLED state below.
    NukeAllSnippets();
    nuke_when_initialized_ = false;
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
      &NTPSnippetsService::OnSnippetsStatusChanged, base::Unretained(this)));

  // Always notify here even if we got nothing from the database, because we
  // don't know how long the fetch will take or if it will even complete.
  for (const auto& item : categories_) {
    Category category = item.first;
    NotifyNewSuggestions(category);
  }
}

void NTPSnippetsService::OnSnippetsStatusChanged(
    SnippetsStatus old_snippets_status,
    SnippetsStatus new_snippets_status) {
  switch (new_snippets_status) {
    case SnippetsStatus::ENABLED_AND_SIGNED_IN:
      if (old_snippets_status == SnippetsStatus::ENABLED_AND_SIGNED_OUT) {
        DCHECK(state_ == State::READY);
        // Clear nonpersonalized suggestions.
        NukeAllSnippets();
        // Fetch personalized ones.
        FetchSnippets(/*interactive_request=*/true);
      } else {
        // Do not change the status. That will be done in EnterStateReady().
        EnterState(State::READY);
      }
      break;

    case SnippetsStatus::ENABLED_AND_SIGNED_OUT:
      if (old_snippets_status == SnippetsStatus::ENABLED_AND_SIGNED_IN) {
        DCHECK(state_ == State::READY);
        // Clear personalized suggestions.
        NukeAllSnippets();
        // Fetch nonpersonalized ones.
        FetchSnippets(/*interactive_request=*/true);
      } else {
        // Do not change the status. That will be done in EnterStateReady().
        EnterState(State::READY);
      }
      break;

    case SnippetsStatus::EXPLICITLY_DISABLED:
      EnterState(State::DISABLED);
      UpdateAllCategoryStatus(CategoryStatus::CATEGORY_EXPLICITLY_DISABLED);
      break;

    case SnippetsStatus::SIGNED_OUT_AND_DISABLED:
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
      break;

    case State::READY:
      DCHECK(state_ == State::NOT_INITED || state_ == State::DISABLED);

      DVLOG(1) << "Entering state: READY";
      state_ = State::READY;
      EnterStateReady();
      break;

    case State::DISABLED:
      DCHECK(state_ == State::NOT_INITED || state_ == State::READY);

      DVLOG(1) << "Entering state: DISABLED";
      state_ = State::DISABLED;
      EnterStateDisabled();
      break;

    case State::ERROR_OCCURRED:
      DVLOG(1) << "Entering state: ERROR_OCCURRED";
      state_ = State::ERROR_OCCURRED;
      EnterStateError();
      break;
  }

  // Schedule or un-schedule background fetching after each state change.
  RescheduleFetching(false);
}

void NTPSnippetsService::NotifyNewSuggestions(Category category) {
  DCHECK(base::ContainsKey(categories_, category));
  const CategoryContent& content = categories_[category];
  DCHECK(IsCategoryStatusAvailable(content.status));

  std::vector<ContentSuggestion> result;
  for (const std::unique_ptr<NTPSnippet>& snippet : content.snippets) {
    // TODO(sfiera): if a snippet is not going to be displayed, move it
    // directly to content.dismissed on fetch. Otherwise, we might prune
    // other snippets to get down to kMaxSnippetCount, only to hide one of the
    // incomplete ones we kept.
    if (!snippet->is_complete())
      continue;
    ContentSuggestion suggestion(category, snippet->id(),
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

  DVLOG(1) << "NotifyNewSuggestions(" << category << "): " << result.size()
           << " items.";
  observer()->OnNewSuggestions(this, category, std::move(result));
}

void NTPSnippetsService::UpdateCategoryStatus(Category category,
                                              CategoryStatus status) {
  DCHECK(base::ContainsKey(categories_, category));
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

const NTPSnippet* NTPSnippetsService::CategoryContent::FindSnippet(
    const std::string& id_within_category) const {
  // Search for the snippet in current and archived snippets.
  auto it = std::find_if(
      snippets.begin(), snippets.end(),
      [&id_within_category](const std::unique_ptr<NTPSnippet>& snippet) {
        return snippet->id() == id_within_category;
      });
  if (it != snippets.end())
    return it->get();

  it = std::find_if(
      archived.begin(), archived.end(),
      [&id_within_category](const std::unique_ptr<NTPSnippet>& snippet) {
        return snippet->id() == id_within_category;
      });
  if (it != archived.end())
    return it->get();

  return nullptr;
}

void NTPSnippetsService::RestoreCategoriesFromPrefs() {
  // This must only be called at startup, before there are any categories.
  DCHECK(categories_.empty());

  const base::ListValue* list =
      pref_service_->GetList(prefs::kRemoteSuggestionCategories);
  for (const std::unique_ptr<base::Value>& entry : *list) {
    const base::DictionaryValue* dict = nullptr;
    if (!entry->GetAsDictionary(&dict)) {
      DLOG(WARNING) << "Invalid category pref value: " << *entry;
      continue;
    }
    int id = 0;
    if (!dict->GetInteger(kCategoryContentId, &id)) {
      DLOG(WARNING) << "Invalid category pref value, missing '"
                    << kCategoryContentId << "': " << *entry;
      continue;
    }
    base::string16 title;
    if (!dict->GetString(kCategoryContentTitle, &title)) {
      DLOG(WARNING) << "Invalid category pref value, missing '"
                    << kCategoryContentTitle << "': " << *entry;
      continue;
    }
    bool provided_by_server = false;
    if (!dict->GetBoolean(kCategoryContentProvidedByServer,
                          &provided_by_server)) {
      DLOG(WARNING) << "Invalid category pref value, missing '"
                    << kCategoryContentProvidedByServer << "': " << *entry;
      continue;
    }

    Category category = category_factory()->FromIDValue(id);
    categories_[category] = CategoryContent();
    categories_[category].localized_title = title;
    categories_[category].provided_by_server = provided_by_server;
  }
}

void NTPSnippetsService::StoreCategoriesToPrefs() {
  // Collect all the CategoryContents.
  std::vector<std::pair<Category, const CategoryContent*>> to_store;
  for (const auto& entry : categories_)
    to_store.emplace_back(entry.first, &entry.second);
  // Sort them into the proper category order.
  std::sort(to_store.begin(), to_store.end(),
            [this](const std::pair<Category, const CategoryContent*>& left,
                   const std::pair<Category, const CategoryContent*>& right) {
              return category_factory()->CompareCategories(left.first,
                                                           right.first);
            });
  // Convert the relevant info into a base::ListValue for storage.
  base::ListValue list;
  for (const auto& entry : to_store) {
    Category category = entry.first;
    const base::string16& title = entry.second->localized_title;
    bool provided_by_server = entry.second->provided_by_server;
    auto dict = base::MakeUnique<base::DictionaryValue>();
    dict->SetInteger(kCategoryContentId, category.id());
    dict->SetString(kCategoryContentTitle, title);
    dict->SetBoolean(kCategoryContentProvidedByServer, provided_by_server);
    list.Append(std::move(dict));
  }
  // Finally, store the result in the pref service.
  pref_service_->Set(prefs::kRemoteSuggestionCategories, list);
}

NTPSnippetsService::CategoryContent::CategoryContent() = default;
NTPSnippetsService::CategoryContent::CategoryContent(CategoryContent&&) =
    default;
NTPSnippetsService::CategoryContent::~CategoryContent() = default;
NTPSnippetsService::CategoryContent& NTPSnippetsService::CategoryContent::
operator=(CategoryContent&&) = default;

}  // namespace ntp_snippets

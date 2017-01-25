// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/history/core/browser/history_service.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

namespace {

// Number of suggestions requested to the server. Consider replacing sparse UMA
// histograms with COUNTS() if this number increases beyond 50.
const int kMaxSnippetCount = 10;

// Number of archived suggestions we keep around in memory.
const int kMaxArchivedSnippetCount = 200;

// Keys for storing CategoryContent info in prefs.
const char kCategoryContentId[] = "id";
const char kCategoryContentTitle[] = "title";
const char kCategoryContentProvidedByServer[] = "provided_by_server";
const char kCategoryContentAllowFetchingMore[] = "allow_fetching_more";

// TODO(treib): Remove after M57.
const char kDeprecatedSnippetHostsPref[] = "ntp_snippets.hosts";

template <typename SnippetPtrContainer>
std::unique_ptr<std::vector<std::string>> GetSnippetIDVector(
    const SnippetPtrContainer& suggestions) {
  auto result = base::MakeUnique<std::vector<std::string>>();
  for (const auto& snippet : suggestions) {
    result->push_back(snippet->id());
  }
  return result;
}

bool HasIntersection(const std::vector<std::string>& a,
                     const std::set<std::string>& b) {
  for (const std::string& item : a) {
    if (base::ContainsValue(b, item)) {
      return true;
    }
  }
  return false;
}

void EraseByPrimaryID(RemoteSuggestion::PtrVector* suggestions,
                      const std::vector<std::string>& ids) {
  std::set<std::string> ids_lookup(ids.begin(), ids.end());
  suggestions->erase(
      std::remove_if(
          suggestions->begin(), suggestions->end(),
          [&ids_lookup](const std::unique_ptr<RemoteSuggestion>& snippet) {
            return base::ContainsValue(ids_lookup, snippet->id());
          }),
      suggestions->end());
}

void EraseMatchingSnippets(RemoteSuggestion::PtrVector* suggestions,
                           const RemoteSuggestion::PtrVector& compare_against) {
  std::set<std::string> compare_against_ids;
  for (const std::unique_ptr<RemoteSuggestion>& snippet : compare_against) {
    const std::vector<std::string>& snippet_ids = snippet->GetAllIDs();
    compare_against_ids.insert(snippet_ids.begin(), snippet_ids.end());
  }
  suggestions->erase(
      std::remove_if(suggestions->begin(), suggestions->end(),
                     [&compare_against_ids](
                         const std::unique_ptr<RemoteSuggestion>& snippet) {
                       return HasIntersection(snippet->GetAllIDs(),
                                              compare_against_ids);
                     }),
      suggestions->end());
}

void RemoveNullPointers(RemoteSuggestion::PtrVector* suggestions) {
  suggestions->erase(
      std::remove_if(suggestions->begin(), suggestions->end(),
                     [](const std::unique_ptr<RemoteSuggestion>& snippet) {
                       return !snippet;
                     }),
      suggestions->end());
}

void RemoveIncompleteSnippets(RemoteSuggestion::PtrVector* suggestions) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAddIncompleteSnippets)) {
    return;
  }
  int num_suggestions = suggestions->size();
  // Remove suggestions that do not have all the info we need to display it to
  // the user.
  suggestions->erase(
      std::remove_if(suggestions->begin(), suggestions->end(),
                     [](const std::unique_ptr<RemoteSuggestion>& snippet) {
                       return !snippet->is_complete();
                     }),
      suggestions->end());
  int num_suggestions_removed = num_suggestions - suggestions->size();
  UMA_HISTOGRAM_BOOLEAN("NewTabPage.Snippets.IncompleteSnippetsAfterFetch",
                        num_suggestions_removed > 0);
  if (num_suggestions_removed > 0) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumIncompleteSnippets",
                                num_suggestions_removed);
  }
}

std::vector<ContentSuggestion> ConvertToContentSuggestions(
    Category category,
    const RemoteSuggestion::PtrVector& suggestions) {
  std::vector<ContentSuggestion> result;
  for (const std::unique_ptr<RemoteSuggestion>& snippet : suggestions) {
    // TODO(sfiera): if a snippet is not going to be displayed, move it
    // directly to content.dismissed on fetch. Otherwise, we might prune
    // other suggestions to get down to kMaxSnippetCount, only to hide one of
    // the
    // incomplete ones we kept.
    if (!snippet->is_complete()) {
      continue;
    }
    result.emplace_back(snippet->ToContentSuggestion(category));
  }
  return result;
}

void CallWithEmptyResults(const FetchDoneCallback& callback,
                          const Status& status) {
  if (callback.is_null()) {
    return;
  }
  callback.Run(status, std::vector<ContentSuggestion>());
}

}  // namespace

CachedImageFetcher::CachedImageFetcher(
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    PrefService* pref_service,
    RemoteSuggestionsDatabase* database)
    : image_fetcher_(std::move(image_fetcher)),
      image_decoder_(std::move(image_decoder)),
      database_(database),
      thumbnail_requests_throttler_(
          pref_service,
          RequestThrottler::RequestType::CONTENT_SUGGESTION_THUMBNAIL) {
  // |image_fetcher_| can be null in tests.
  if (image_fetcher_) {
    image_fetcher_->SetImageFetcherDelegate(this);
    image_fetcher_->SetDataUseServiceName(
        data_use_measurement::DataUseUserData::NTP_SNIPPETS);
  }
}

CachedImageFetcher::~CachedImageFetcher() {}

void CachedImageFetcher::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    const ImageFetchedCallback& callback) {
  database_->LoadImage(
      suggestion_id.id_within_category(),
      base::Bind(&CachedImageFetcher::OnSnippetImageFetchedFromDatabase,
                 base::Unretained(this), callback, suggestion_id, url));
}

// This function gets only called for caching the image data received from the
// network. The actual decoding is done in OnSnippetImageDecodedFromDatabase().
void CachedImageFetcher::OnImageDataFetched(
    const std::string& id_within_category,
    const std::string& image_data) {
  if (image_data.empty()) {
    return;
  }
  database_->SaveImage(id_within_category, image_data);
}

void CachedImageFetcher::OnImageDecodingDone(
    const ImageFetchedCallback& callback,
    const std::string& id_within_category,
    const gfx::Image& image) {
  callback.Run(image);
}

void CachedImageFetcher::OnSnippetImageFetchedFromDatabase(
    const ImageFetchedCallback& callback,
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    std::string data) {  // SnippetImageCallback requires nonconst reference.
  // |image_decoder_| is null in tests.
  if (image_decoder_ && !data.empty()) {
    image_decoder_->DecodeImage(
        data, base::Bind(&CachedImageFetcher::OnSnippetImageDecodedFromDatabase,
                         base::Unretained(this), callback, suggestion_id, url));
    return;
  }
  // Fetching from the DB failed; start a network fetch.
  FetchSnippetImageFromNetwork(suggestion_id, url, callback);
}

void CachedImageFetcher::OnSnippetImageDecodedFromDatabase(
    const ImageFetchedCallback& callback,
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    const gfx::Image& image) {
  if (!image.IsEmpty()) {
    callback.Run(image);
    return;
  }
  // If decoding the image failed, delete the DB entry.
  database_->DeleteImage(suggestion_id.id_within_category());
  FetchSnippetImageFromNetwork(suggestion_id, url, callback);
}

void CachedImageFetcher::FetchSnippetImageFromNetwork(
    const ContentSuggestion::ID& suggestion_id,
    const GURL& url,
    const ImageFetchedCallback& callback) {
  if (url.is_empty() ||
      !thumbnail_requests_throttler_.DemandQuotaForRequest(
          /*interactive_request=*/true)) {
    // Return an empty image. Directly, this is never synchronous with the
    // original FetchSuggestionImage() call - an asynchronous database query has
    // happened in the meantime.
    callback.Run(gfx::Image());
    return;
  }

  image_fetcher_->StartOrQueueNetworkRequest(
      suggestion_id.id_within_category(), url,
      base::Bind(&CachedImageFetcher::OnImageDecodingDone,
                 base::Unretained(this), callback));
}

RemoteSuggestionsProviderImpl::RemoteSuggestionsProviderImpl(
    Observer* observer,
    PrefService* pref_service,
    const std::string& application_language_code,
    CategoryRanker* category_ranker,
    std::unique_ptr<RemoteSuggestionsFetcher> suggestions_fetcher,
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    std::unique_ptr<RemoteSuggestionsDatabase> database,
    std::unique_ptr<RemoteSuggestionsStatusService> status_service)
    : RemoteSuggestionsProvider(observer),
      state_(State::NOT_INITED),
      pref_service_(pref_service),
      articles_category_(
          Category::FromKnownCategory(KnownCategories::ARTICLES)),
      application_language_code_(application_language_code),
      category_ranker_(category_ranker),
      suggestions_fetcher_(std::move(suggestions_fetcher)),
      database_(std::move(database)),
      image_fetcher_(std::move(image_fetcher),
                     std::move(image_decoder),
                     pref_service,
                     database_.get()),
      status_service_(std::move(status_service)),
      fetch_when_ready_(false),
      fetch_when_ready_interactive_(false),
      fetch_when_ready_callback_(nullptr),
      provider_status_callback_(nullptr),
      nuke_when_initialized_(false),
      clock_(base::MakeUnique<base::DefaultClock>()) {
  pref_service_->ClearPref(kDeprecatedSnippetHostsPref);

  RestoreCategoriesFromPrefs();
  // The articles category always exists. Add it if we didn't get it from prefs.
  // TODO(treib): Rethink this.
  category_contents_.insert(
      std::make_pair(articles_category_,
                     CategoryContent(BuildArticleCategoryInfo(base::nullopt))));
  // Tell the observer about all the categories.
  for (const auto& entry : category_contents_) {
    observer->OnCategoryStatusChanged(this, entry.first, entry.second.status);
  }

  if (database_->IsErrorState()) {
    EnterState(State::ERROR_OCCURRED);
    UpdateAllCategoryStatus(CategoryStatus::LOADING_ERROR);
    return;
  }

  database_->SetErrorCallback(base::Bind(
      &RemoteSuggestionsProviderImpl::OnDatabaseError, base::Unretained(this)));

  // We transition to other states while finalizing the initialization, when the
  // database is done loading.
  database_load_start_ = base::TimeTicks::Now();
  database_->LoadSnippets(
      base::Bind(&RemoteSuggestionsProviderImpl::OnDatabaseLoaded,
                 base::Unretained(this)));
}

RemoteSuggestionsProviderImpl::~RemoteSuggestionsProviderImpl() = default;

// static
void RemoteSuggestionsProviderImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  // TODO(treib): Remove after M57.
  registry->RegisterListPref(kDeprecatedSnippetHostsPref);
  registry->RegisterListPref(prefs::kRemoteSuggestionCategories);
  registry->RegisterInt64Pref(prefs::kLastSuccessfulBackgroundFetchTime, 0);

  RemoteSuggestionsStatusService::RegisterProfilePrefs(registry);
}

void RemoteSuggestionsProviderImpl::SetProviderStatusCallback(
    std::unique_ptr<ProviderStatusCallback> callback) {
  provider_status_callback_ = std::move(callback);
  // Call the observer right away if we've reached any final state.
  NotifyStateChanged();
}

void RemoteSuggestionsProviderImpl::ReloadSuggestions() {
  FetchSnippets(/*interactive_request=*/true,
                /*callback=*/nullptr);
}

void RemoteSuggestionsProviderImpl::RefetchInTheBackground(
    std::unique_ptr<FetchStatusCallback> callback) {
  FetchSnippets(/*interactive_request=*/false, std::move(callback));
}

const RemoteSuggestionsFetcher*
RemoteSuggestionsProviderImpl::suggestions_fetcher_for_debugging() const {
  return suggestions_fetcher_.get();
}

void RemoteSuggestionsProviderImpl::FetchSnippets(
    bool interactive_request,
    std::unique_ptr<FetchStatusCallback> callback) {
  if (!ready()) {
    fetch_when_ready_ = true;
    fetch_when_ready_interactive_ = interactive_request;
    fetch_when_ready_callback_ = std::move(callback);
    return;
  }

  MarkEmptyCategoriesAsLoading();

  RequestParams params = BuildFetchParams();
  params.interactive_request = interactive_request;
  suggestions_fetcher_->FetchSnippets(
      params, base::BindOnce(&RemoteSuggestionsProviderImpl::OnFetchFinished,
                             base::Unretained(this), std::move(callback),
                             interactive_request));
}

void RemoteSuggestionsProviderImpl::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  if (!ready()) {
    CallWithEmptyResults(callback,
                         Status(StatusCode::TEMPORARY_ERROR,
                                "RemoteSuggestionsProvider is not ready!"));
    return;
  }
  RequestParams params = BuildFetchParams();
  params.excluded_ids.insert(known_suggestion_ids.begin(),
                             known_suggestion_ids.end());
  params.interactive_request = true;
  params.exclusive_category = category;

  suggestions_fetcher_->FetchSnippets(
      params,
      base::BindOnce(&RemoteSuggestionsProviderImpl::OnFetchMoreFinished,
                     base::Unretained(this), callback));
}

// Builds default fetcher params.
RequestParams RemoteSuggestionsProviderImpl::BuildFetchParams() const {
  RequestParams result;
  result.language_code = application_language_code_;
  result.count_to_fetch = kMaxSnippetCount;
  for (const auto& map_entry : category_contents_) {
    const CategoryContent& content = map_entry.second;
    for (const auto& dismissed_snippet : content.dismissed) {
      result.excluded_ids.insert(dismissed_snippet->id());
    }
  }
  return result;
}

void RemoteSuggestionsProviderImpl::MarkEmptyCategoriesAsLoading() {
  for (const auto& item : category_contents_) {
    Category category = item.first;
    const CategoryContent& content = item.second;
    if (content.suggestions.empty()) {
      UpdateCategoryStatus(category, CategoryStatus::AVAILABLE_LOADING);
    }
  }
}

CategoryStatus RemoteSuggestionsProviderImpl::GetCategoryStatus(
    Category category) {
  auto content_it = category_contents_.find(category);
  DCHECK(content_it != category_contents_.end());
  return content_it->second.status;
}

CategoryInfo RemoteSuggestionsProviderImpl::GetCategoryInfo(Category category) {
  auto content_it = category_contents_.find(category);
  DCHECK(content_it != category_contents_.end());
  return content_it->second.info;
}

void RemoteSuggestionsProviderImpl::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  if (!ready()) {
    return;
  }

  auto content_it = category_contents_.find(suggestion_id.category());
  DCHECK(content_it != category_contents_.end());
  CategoryContent* content = &content_it->second;
  DismissSuggestionFromCategoryContent(content,
                                       suggestion_id.id_within_category());
}

void RemoteSuggestionsProviderImpl::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  // Both time range and the filter are ignored and all suggestions are removed,
  // because it is not known which history entries were used for the suggestions
  // personalization.
  if (!ready()) {
    // No need to refresh the UI afterwards as we didn't provide any data to the
    // UI so far.
    nuke_when_initialized_ = true;
  } else {
    NukeAllSnippets();
  }
}

void RemoteSuggestionsProviderImpl::ClearCachedSuggestions(Category category) {
  if (!initialized()) {
    return;
  }

  auto content_it = category_contents_.find(category);
  if (content_it == category_contents_.end()) {
    return;
  }
  CategoryContent* content = &content_it->second;
  // TODO(tschumann): We do the unnecessary checks for .empty() in many places
  // before calling database methods. Change the RemoteSuggestionsDatabase to
  // return early for those and remove the many if statements in this file.
  if (!content->suggestions.empty()) {
    database_->DeleteSnippets(GetSnippetIDVector(content->suggestions));
    database_->DeleteImages(GetSnippetIDVector(content->suggestions));
    content->suggestions.clear();
  }
  if (!content->archived.empty()) {
    database_->DeleteSnippets(GetSnippetIDVector(content->archived));
    database_->DeleteImages(GetSnippetIDVector(content->archived));
    content->archived.clear();
  }
}

void RemoteSuggestionsProviderImpl::OnSignInStateChanged() {
  // Make sure the status service is registered and we already initialised its
  // start state.
  if (!initialized()) {
    return;
  }

  status_service_->OnSignInStateChanged();
}

void RemoteSuggestionsProviderImpl::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  auto content_it = category_contents_.find(category);
  DCHECK(content_it != category_contents_.end());
  callback.Run(
      ConvertToContentSuggestions(category, content_it->second.dismissed));
}

void RemoteSuggestionsProviderImpl::ClearDismissedSuggestionsForDebugging(
    Category category) {
  auto content_it = category_contents_.find(category);
  DCHECK(content_it != category_contents_.end());
  CategoryContent* content = &content_it->second;

  if (!initialized()) {
    return;
  }

  if (content->dismissed.empty()) {
    return;
  }

  database_->DeleteSnippets(GetSnippetIDVector(content->dismissed));
  // The image got already deleted when the suggestion was dismissed.

  content->dismissed.clear();
}

// static
int RemoteSuggestionsProviderImpl::GetMaxSnippetCountForTesting() {
  return kMaxSnippetCount;
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

GURL RemoteSuggestionsProviderImpl::FindSnippetImageUrl(
    const ContentSuggestion::ID& suggestion_id) const {
  DCHECK(base::ContainsKey(category_contents_, suggestion_id.category()));

  const CategoryContent& content =
      category_contents_.at(suggestion_id.category());
  const RemoteSuggestion* snippet =
      content.FindSnippet(suggestion_id.id_within_category());
  if (!snippet) {
    return GURL();
  }
  return snippet->salient_image_url();
}

void RemoteSuggestionsProviderImpl::OnDatabaseLoaded(
    RemoteSuggestion::PtrVector suggestions) {
  if (state_ == State::ERROR_OCCURRED) {
    return;
  }
  DCHECK(state_ == State::NOT_INITED);
  DCHECK(base::ContainsKey(category_contents_, articles_category_));

  base::TimeDelta database_load_time =
      base::TimeTicks::Now() - database_load_start_;
  UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.Snippets.DatabaseLoadTime",
                             database_load_time);

  RemoteSuggestion::PtrVector to_delete;
  for (std::unique_ptr<RemoteSuggestion>& snippet : suggestions) {
    Category snippet_category =
        Category::FromRemoteCategory(snippet->remote_category_id());
    auto content_it = category_contents_.find(snippet_category);
    // We should already know about the category.
    if (content_it == category_contents_.end()) {
      DLOG(WARNING) << "Loaded a suggestion for unknown category "
                    << snippet_category << " from the DB; deleting";
      to_delete.emplace_back(std::move(snippet));
      continue;
    }
    CategoryContent* content = &content_it->second;
    if (snippet->is_dismissed()) {
      content->dismissed.emplace_back(std::move(snippet));
    } else {
      content->suggestions.emplace_back(std::move(snippet));
    }
  }
  if (!to_delete.empty()) {
    database_->DeleteSnippets(GetSnippetIDVector(to_delete));
    database_->DeleteImages(GetSnippetIDVector(to_delete));
  }

  // Sort the suggestions in each category.
  // TODO(treib): Persist the actual order in the DB somehow? crbug.com/654409
  for (auto& entry : category_contents_) {
    CategoryContent* content = &entry.second;
    std::sort(content->suggestions.begin(), content->suggestions.end(),
              [](const std::unique_ptr<RemoteSuggestion>& lhs,
                 const std::unique_ptr<RemoteSuggestion>& rhs) {
                return lhs->score() > rhs->score();
              });
  }

  // TODO(tschumann): If I move ClearExpiredDismissedSnippets() to the beginning
  // of the function, it essentially does nothing but tests are still green. Fix
  // this!
  ClearExpiredDismissedSnippets();
  ClearOrphanedImages();
  FinishInitialization();
}

void RemoteSuggestionsProviderImpl::OnDatabaseError() {
  EnterState(State::ERROR_OCCURRED);
  UpdateAllCategoryStatus(CategoryStatus::LOADING_ERROR);
}

void RemoteSuggestionsProviderImpl::OnFetchMoreFinished(
    const FetchDoneCallback& fetching_callback,
    Status status,
    RemoteSuggestionsFetcher::OptionalFetchedCategories fetched_categories) {
  if (!fetched_categories) {
    DCHECK(!status.IsSuccess());
    CallWithEmptyResults(fetching_callback, status);
    return;
  }
  if (fetched_categories->size() != 1u) {
    LOG(DFATAL) << "Requested one exclusive category but received "
                << fetched_categories->size() << " categories.";
    CallWithEmptyResults(fetching_callback,
                         Status(StatusCode::PERMANENT_ERROR,
                                "RemoteSuggestionsProvider received more "
                                "categories than requested."));
    return;
  }
  auto& fetched_category = (*fetched_categories)[0];
  Category category = fetched_category.category;
  CategoryContent* existing_content =
      UpdateCategoryInfo(category, fetched_category.info);
  SanitizeReceivedSnippets(existing_content->dismissed,
                           &fetched_category.suggestions);
  // We compute the result now before modifying |fetched_category.suggestions|.
  // However, we wait with notifying the caller until the end of the method when
  // all state is updated.
  std::vector<ContentSuggestion> result =
      ConvertToContentSuggestions(category, fetched_category.suggestions);

  // Fill up the newly fetched suggestions with existing ones, store them, and
  // notify observers about new data.
  while (fetched_category.suggestions.size() <
             static_cast<size_t>(kMaxSnippetCount) &&
         !existing_content->suggestions.empty()) {
    fetched_category.suggestions.emplace(
        fetched_category.suggestions.begin(),
        std::move(existing_content->suggestions.back()));
    existing_content->suggestions.pop_back();
  }
  std::vector<std::string> to_dismiss =
      *GetSnippetIDVector(existing_content->suggestions);
  for (const auto& id : to_dismiss) {
    DismissSuggestionFromCategoryContent(existing_content, id);
  }
  DCHECK(existing_content->suggestions.empty());

  IntegrateSnippets(existing_content, std::move(fetched_category.suggestions));

  // TODO(tschumann): We should properly honor the existing category state,
  // e.g. to make sure we don't serve results after the sign-out. Revisit this:
  // Should Nuke also cancel outstanding requests, or do we want to check the
  // status?
  UpdateCategoryStatus(category, CategoryStatus::AVAILABLE);
  // Notify callers and observers.
  fetching_callback.Run(Status::Success(), std::move(result));
  NotifyNewSuggestions(category, *existing_content);
}

void RemoteSuggestionsProviderImpl::OnFetchFinished(
    std::unique_ptr<FetchStatusCallback> callback,
    bool interactive_request,
    Status status,
    RemoteSuggestionsFetcher::OptionalFetchedCategories fetched_categories) {
  if (!ready()) {
    // TODO(tschumann): What happens if this was a user-triggered, interactive
    // request? Is the UI waiting indefinitely now?
    return;
  }

  // Record the fetch time of a successfull background fetch.
  if (!interactive_request && status.IsSuccess()) {
    pref_service_->SetInt64(prefs::kLastSuccessfulBackgroundFetchTime,
                            clock_->Now().ToInternalValue());
  }

  // Mark all categories as not provided by the server in the latest fetch. The
  // ones we got will be marked again below.
  for (auto& item : category_contents_) {
    CategoryContent* content = &item.second;
    content->included_in_last_server_response = false;
  }

  // Clear up expired dismissed suggestions before we use them to filter new
  // ones.
  ClearExpiredDismissedSnippets();

  // If suggestions were fetched successfully, update our |category_contents_|
  // from
  // each category provided by the server.
  if (fetched_categories) {
    // TODO(treib): Reorder |category_contents_| to match the order we received
    // from the server. crbug.com/653816
    for (RemoteSuggestionsFetcher::FetchedCategory& fetched_category :
         *fetched_categories) {
      // TODO(tschumann): Remove this histogram once we only talk to the content
      // suggestions cloud backend.
      if (fetched_category.category == articles_category_) {
        UMA_HISTOGRAM_SPARSE_SLOWLY(
            "NewTabPage.Snippets.NumArticlesFetched",
            std::min(fetched_category.suggestions.size(),
                     static_cast<size_t>(kMaxSnippetCount + 1)));
      }
      category_ranker_->AppendCategoryIfNecessary(fetched_category.category);
      CategoryContent* content =
          UpdateCategoryInfo(fetched_category.category, fetched_category.info);
      content->included_in_last_server_response = true;
      SanitizeReceivedSnippets(content->dismissed,
                               &fetched_category.suggestions);
      IntegrateSnippets(content, std::move(fetched_category.suggestions));
    }
  }

  // TODO(tschumann): The suggestions fetcher needs to signal errors so that we
  // know why we received no data. If an error occured, none of the following
  // should take place.

  // We might have gotten new categories (or updated the titles of existing
  // ones), so update the pref.
  StoreCategoriesToPrefs();

  for (const auto& item : category_contents_) {
    Category category = item.first;
    UpdateCategoryStatus(category, CategoryStatus::AVAILABLE);
    // TODO(sfiera): notify only when a category changed above.
    NotifyNewSuggestions(category, item.second);
  }

  // TODO(sfiera): equivalent metrics for non-articles.
  auto content_it = category_contents_.find(articles_category_);
  DCHECK(content_it != category_contents_.end());
  const CategoryContent& content = content_it->second;
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.NumArticles",
                              content.suggestions.size());
  if (content.suggestions.empty() && !content.dismissed.empty()) {
    UMA_HISTOGRAM_COUNTS("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded",
                         content.dismissed.size());
  }

  if (callback) {
    callback->Run(status);
  }
}

void RemoteSuggestionsProviderImpl::ArchiveSnippets(
    CategoryContent* content,
    RemoteSuggestion::PtrVector* to_archive) {
  // Archive previous suggestions - move them at the beginning of the list.
  content->archived.insert(content->archived.begin(),
                           std::make_move_iterator(to_archive->begin()),
                           std::make_move_iterator(to_archive->end()));
  to_archive->clear();

  // If there are more archived suggestions than we want to keep, delete the
  // oldest ones by their fetch time (which are always in the back).
  if (content->archived.size() > kMaxArchivedSnippetCount) {
    RemoteSuggestion::PtrVector to_delete(
        std::make_move_iterator(content->archived.begin() +
                                kMaxArchivedSnippetCount),
        std::make_move_iterator(content->archived.end()));
    content->archived.resize(kMaxArchivedSnippetCount);
    database_->DeleteImages(GetSnippetIDVector(to_delete));
  }
}

void RemoteSuggestionsProviderImpl::SanitizeReceivedSnippets(
    const RemoteSuggestion::PtrVector& dismissed,
    RemoteSuggestion::PtrVector* suggestions) {
  DCHECK(ready());
  EraseMatchingSnippets(suggestions, dismissed);
  RemoveIncompleteSnippets(suggestions);
}

void RemoteSuggestionsProviderImpl::IntegrateSnippets(
    CategoryContent* content,
    RemoteSuggestion::PtrVector new_suggestions) {
  DCHECK(ready());

  // Do not touch the current set of suggestions if the newly fetched one is
  // empty.
  // TODO(tschumann): This should go. If we get empty results we should update
  // accordingly and remove the old one (only of course if this was not received
  // through a fetch-more).
  if (new_suggestions.empty()) {
    return;
  }

  // It's entirely possible that the newly fetched suggestions contain articles
  // that have been present before.
  // We need to make sure to only delete and archive suggestions that don't
  // appear with the same ID in the new suggestions (it's fine for additional
  // IDs though).
  EraseByPrimaryID(&content->suggestions, *GetSnippetIDVector(new_suggestions));
  // Do not delete the thumbnail images as they are still handy on open NTPs.
  database_->DeleteSnippets(GetSnippetIDVector(content->suggestions));
  // Note, that ArchiveSnippets will clear |content->suggestions|.
  ArchiveSnippets(content, &content->suggestions);

  database_->SaveSnippets(new_suggestions);

  content->suggestions = std::move(new_suggestions);
}

void RemoteSuggestionsProviderImpl::DismissSuggestionFromCategoryContent(
    CategoryContent* content,
    const std::string& id_within_category) {
  auto it = std::find_if(
      content->suggestions.begin(), content->suggestions.end(),
      [&id_within_category](const std::unique_ptr<RemoteSuggestion>& snippet) {
        return snippet->id() == id_within_category;
      });
  if (it == content->suggestions.end()) {
    return;
  }

  (*it)->set_dismissed(true);

  database_->SaveSnippet(**it);

  content->dismissed.push_back(std::move(*it));
  content->suggestions.erase(it);
}

void RemoteSuggestionsProviderImpl::ClearExpiredDismissedSnippets() {
  std::vector<Category> categories_to_erase;

  const base::Time now = base::Time::Now();

  for (auto& item : category_contents_) {
    Category category = item.first;
    CategoryContent* content = &item.second;

    RemoteSuggestion::PtrVector to_delete;
    // Move expired dismissed suggestions over into |to_delete|.
    for (std::unique_ptr<RemoteSuggestion>& snippet : content->dismissed) {
      if (snippet->expiry_date() <= now) {
        to_delete.emplace_back(std::move(snippet));
      }
    }
    RemoveNullPointers(&content->dismissed);

    // Delete the images.
    database_->DeleteImages(GetSnippetIDVector(to_delete));
    // Delete the removed article suggestions from the DB.
    database_->DeleteSnippets(GetSnippetIDVector(to_delete));

    if (content->suggestions.empty() && content->dismissed.empty() &&
        category != articles_category_ &&
        !content->included_in_last_server_response) {
      categories_to_erase.push_back(category);
    }
  }

  for (Category category : categories_to_erase) {
    UpdateCategoryStatus(category, CategoryStatus::NOT_PROVIDED);
    category_contents_.erase(category);
  }

  StoreCategoriesToPrefs();
}

void RemoteSuggestionsProviderImpl::ClearOrphanedImages() {
  auto alive_suggestions = base::MakeUnique<std::set<std::string>>();
  for (const auto& entry : category_contents_) {
    const CategoryContent& content = entry.second;
    for (const auto& snippet_ptr : content.suggestions) {
      alive_suggestions->insert(snippet_ptr->id());
    }
    for (const auto& snippet_ptr : content.dismissed) {
      alive_suggestions->insert(snippet_ptr->id());
    }
  }
  database_->GarbageCollectImages(std::move(alive_suggestions));
}

void RemoteSuggestionsProviderImpl::NukeAllSnippets() {
  for (const auto& item : category_contents_) {
    Category category = item.first;
    const CategoryContent& content = item.second;

    ClearCachedSuggestions(category);
    // Update listeners about the new (empty) state.
    if (IsCategoryStatusAvailable(content.status)) {
      NotifyNewSuggestions(category, content);
    }
    // TODO(tschumann): We should not call debug code from production code.
    ClearDismissedSuggestionsForDebugging(category);
  }

  StoreCategoriesToPrefs();
}

void RemoteSuggestionsProviderImpl::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  if (!base::ContainsKey(category_contents_, suggestion_id.category())) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, gfx::Image()));
    return;
  }
  GURL image_url = FindSnippetImageUrl(suggestion_id);
  if (image_url.is_empty()) {
    // As we don't know the corresponding snippet anymore, we don't expect to
    // find it in the database (and also can't fetch it remotely). Cut the
    // lookup short and return directly.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, gfx::Image()));
    return;
  }
  image_fetcher_.FetchSuggestionImage(suggestion_id, image_url, callback);
}

void RemoteSuggestionsProviderImpl::EnterStateReady() {
  if (nuke_when_initialized_) {
    NukeAllSnippets();
    nuke_when_initialized_ = false;
  }

  auto article_category_it = category_contents_.find(articles_category_);
  DCHECK(article_category_it != category_contents_.end());
  if (article_category_it->second.suggestions.empty() || fetch_when_ready_) {
    // TODO(jkrcal): Fetching suggestions automatically upon creation of this
    // lazily created service can cause troubles, e.g. in unit tests where
    // network I/O is not allowed.
    // Either add a DCHECK here that we actually are allowed to do network I/O
    // or change the logic so that some explicit call is always needed for the
    // network request.
    FetchSnippets(fetch_when_ready_interactive_,
                  std::move(fetch_when_ready_callback_));
    fetch_when_ready_ = false;
  }

  for (const auto& item : category_contents_) {
    Category category = item.first;
    const CategoryContent& content = item.second;
    // FetchSnippets has set the status to |AVAILABLE_LOADING| if relevant,
    // otherwise we transition to |AVAILABLE| here.
    if (content.status != CategoryStatus::AVAILABLE_LOADING) {
      UpdateCategoryStatus(category, CategoryStatus::AVAILABLE);
    }
  }
}

void RemoteSuggestionsProviderImpl::EnterStateDisabled() {
  NukeAllSnippets();
}

void RemoteSuggestionsProviderImpl::EnterStateError() {
  status_service_.reset();
}

void RemoteSuggestionsProviderImpl::FinishInitialization() {
  if (nuke_when_initialized_) {
    // We nuke here in addition to EnterStateReady, so that it happens even if
    // we enter the DISABLED state below.
    NukeAllSnippets();
    nuke_when_initialized_ = false;
  }

  // Note: Initializing the status service will run the callback right away with
  // the current state.
  status_service_->Init(base::Bind(
      &RemoteSuggestionsProviderImpl::OnStatusChanged, base::Unretained(this)));

  // Always notify here even if we got nothing from the database, because we
  // don't know how long the fetch will take or if it will even complete.
  for (const auto& item : category_contents_) {
    Category category = item.first;
    const CategoryContent& content = item.second;
    // Note: We might be in a non-available status here, e.g. DISABLED due to
    // enterprise policy.
    if (IsCategoryStatusAvailable(content.status)) {
      NotifyNewSuggestions(category, content);
    }
  }
}

void RemoteSuggestionsProviderImpl::OnStatusChanged(
    RemoteSuggestionsStatus old_status,
    RemoteSuggestionsStatus new_status) {
  switch (new_status) {
    case RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN:
      if (old_status == RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT) {
        DCHECK(state_ == State::READY);
        // Clear nonpersonalized suggestions.
        NukeAllSnippets();
        // Fetch personalized ones.
        // TODO(jkrcal): Loop in SchedulingRemoteSuggestionsProvider somehow.
        FetchSnippets(/*interactive_request=*/true,
                      /*callback=*/nullptr);
      } else {
        // Do not change the status. That will be done in EnterStateReady().
        EnterState(State::READY);
      }
      break;

    case RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT:
      if (old_status == RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN) {
        DCHECK(state_ == State::READY);
        // Clear personalized suggestions.
        NukeAllSnippets();
        // Fetch nonpersonalized ones.
        // TODO(jkrcal): Loop in SchedulingRemoteSuggestionsProvider somehow.
        FetchSnippets(/*interactive_request=*/true,
                      /*callback=*/nullptr);
      } else {
        // Do not change the status. That will be done in EnterStateReady().
        EnterState(State::READY);
      }
      break;

    case RemoteSuggestionsStatus::EXPLICITLY_DISABLED:
      EnterState(State::DISABLED);
      UpdateAllCategoryStatus(CategoryStatus::CATEGORY_EXPLICITLY_DISABLED);
      break;

    case RemoteSuggestionsStatus::SIGNED_OUT_AND_DISABLED:
      EnterState(State::DISABLED);
      UpdateAllCategoryStatus(CategoryStatus::SIGNED_OUT);
      break;
  }
}

void RemoteSuggestionsProviderImpl::EnterState(State state) {
  if (state == state_) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Snippets.EnteredState",
                            static_cast<int>(state),
                            static_cast<int>(State::COUNT));

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

    case State::COUNT:
      NOTREACHED();
      break;
  }

  NotifyStateChanged();
}

void RemoteSuggestionsProviderImpl::NotifyStateChanged() {
  if (!provider_status_callback_) {
    return;
  }

  switch (state_) {
    case State::NOT_INITED:
      // Initial state, not sure yet whether active or not.
      break;
    case State::READY:
      provider_status_callback_->Run(ProviderStatus::ACTIVE);
      break;
    case State::DISABLED:
      provider_status_callback_->Run(ProviderStatus::INACTIVE);
      break;
    case State::ERROR_OCCURRED:
      provider_status_callback_->Run(ProviderStatus::INACTIVE);
      break;
    case State::COUNT:
      NOTREACHED();
      break;
  }
}

void RemoteSuggestionsProviderImpl::NotifyNewSuggestions(
    Category category,
    const CategoryContent& content) {
  DCHECK(IsCategoryStatusAvailable(content.status));

  std::vector<ContentSuggestion> result =
      ConvertToContentSuggestions(category, content.suggestions);

  DVLOG(1) << "NotifyNewSuggestions(): " << result.size()
           << " items in category " << category;
  observer()->OnNewSuggestions(this, category, std::move(result));
}

void RemoteSuggestionsProviderImpl::UpdateCategoryStatus(
    Category category,
    CategoryStatus status) {
  auto content_it = category_contents_.find(category);
  DCHECK(content_it != category_contents_.end());
  CategoryContent& content = content_it->second;

  if (status == content.status) {
    return;
  }

  DVLOG(1) << "UpdateCategoryStatus(): " << category.id() << ": "
           << static_cast<int>(content.status) << " -> "
           << static_cast<int>(status);
  content.status = status;
  observer()->OnCategoryStatusChanged(this, category, content.status);
}

void RemoteSuggestionsProviderImpl::UpdateAllCategoryStatus(
    CategoryStatus status) {
  for (const auto& category : category_contents_) {
    UpdateCategoryStatus(category.first, status);
  }
}

namespace {

template <typename T>
typename T::const_iterator FindSnippetInContainer(
    const T& container,
    const std::string& id_within_category) {
  return std::find_if(
      container.begin(), container.end(),
      [&id_within_category](const std::unique_ptr<RemoteSuggestion>& snippet) {
        return snippet->id() == id_within_category;
      });
}

}  // namespace

const RemoteSuggestion*
RemoteSuggestionsProviderImpl::CategoryContent::FindSnippet(
    const std::string& id_within_category) const {
  // Search for the suggestion in current and archived suggestions.
  auto it = FindSnippetInContainer(suggestions, id_within_category);
  if (it != suggestions.end()) {
    return it->get();
  }
  auto archived_it = FindSnippetInContainer(archived, id_within_category);
  if (archived_it != archived.end()) {
    return archived_it->get();
  }
  auto dismissed_it = FindSnippetInContainer(dismissed, id_within_category);
  if (dismissed_it != dismissed.end()) {
    return dismissed_it->get();
  }
  return nullptr;
}

RemoteSuggestionsProviderImpl::CategoryContent*
RemoteSuggestionsProviderImpl::UpdateCategoryInfo(Category category,
                                                  const CategoryInfo& info) {
  auto content_it = category_contents_.find(category);
  if (content_it == category_contents_.end()) {
    content_it = category_contents_
                     .insert(std::make_pair(category, CategoryContent(info)))
                     .first;
  } else {
    content_it->second.info = info;
  }
  return &content_it->second;
}

void RemoteSuggestionsProviderImpl::RestoreCategoriesFromPrefs() {
  // This must only be called at startup, before there are any categories.
  DCHECK(category_contents_.empty());

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
    bool included_in_last_server_response = false;
    if (!dict->GetBoolean(kCategoryContentProvidedByServer,
                          &included_in_last_server_response)) {
      DLOG(WARNING) << "Invalid category pref value, missing '"
                    << kCategoryContentProvidedByServer << "': " << *entry;
      continue;
    }
    bool allow_fetching_more_results = false;
    // This wasn't always around, so it's okay if it's missing.
    dict->GetBoolean(kCategoryContentAllowFetchingMore,
                     &allow_fetching_more_results);

    Category category = Category::FromIDValue(id);
    // The ranker may not persist the order of remote categories.
    category_ranker_->AppendCategoryIfNecessary(category);
    // TODO(tschumann): The following has a bad smell that category
    // serialization / deserialization should not be done inside this
    // class. We should move that into a central place that also knows how to
    // parse data we received from remote backends.
    CategoryInfo info =
        category == articles_category_
            ? BuildArticleCategoryInfo(title)
            : BuildRemoteCategoryInfo(title, allow_fetching_more_results);
    CategoryContent* content = UpdateCategoryInfo(category, info);
    content->included_in_last_server_response =
        included_in_last_server_response;
  }
}

void RemoteSuggestionsProviderImpl::StoreCategoriesToPrefs() {
  // Collect all the CategoryContents.
  std::vector<std::pair<Category, const CategoryContent*>> to_store;
  for (const auto& entry : category_contents_) {
    to_store.emplace_back(entry.first, &entry.second);
  }
  // The ranker may not persist the order, thus, it is stored by the provider.
  std::sort(to_store.begin(), to_store.end(),
            [this](const std::pair<Category, const CategoryContent*>& left,
                   const std::pair<Category, const CategoryContent*>& right) {
              return category_ranker_->Compare(left.first, right.first);
            });
  // Convert the relevant info into a base::ListValue for storage.
  base::ListValue list;
  for (const auto& entry : to_store) {
    const Category& category = entry.first;
    const CategoryContent& content = *entry.second;
    auto dict = base::MakeUnique<base::DictionaryValue>();
    dict->SetInteger(kCategoryContentId, category.id());
    // TODO(tschumann): Persist other properties of the CategoryInfo.
    dict->SetString(kCategoryContentTitle, content.info.title());
    dict->SetBoolean(kCategoryContentProvidedByServer,
                     content.included_in_last_server_response);
    dict->SetBoolean(kCategoryContentAllowFetchingMore,
                     content.info.has_more_action());
    list.Append(std::move(dict));
  }
  // Finally, store the result in the pref service.
  pref_service_->Set(prefs::kRemoteSuggestionCategories, list);
}

RemoteSuggestionsProviderImpl::CategoryContent::CategoryContent(
    const CategoryInfo& info)
    : info(info) {}

RemoteSuggestionsProviderImpl::CategoryContent::CategoryContent(
    CategoryContent&&) = default;

RemoteSuggestionsProviderImpl::CategoryContent::~CategoryContent() = default;

RemoteSuggestionsProviderImpl::CategoryContent&
RemoteSuggestionsProviderImpl::CategoryContent::operator=(CategoryContent&&) =
    default;

}  // namespace ntp_snippets

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/download_suggestions_provider.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/pref_util.h"
#include "components/offline_pages/core/offline_page_model_query.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using base::ContainsValue;
using content::DownloadItem;
using content::DownloadManager;
using ntp_snippets::Category;
using ntp_snippets::CategoryInfo;
using ntp_snippets::CategoryStatus;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::prefs::kDismissedAssetDownloadSuggestions;
using ntp_snippets::prefs::kDismissedOfflinePageDownloadSuggestions;
using offline_pages::OfflinePageItem;
using offline_pages::OfflinePageModelQuery;
using offline_pages::OfflinePageModelQueryBuilder;

namespace {

const int kDefaultMaxSuggestionsCount = 5;
const char kAssetDownloadsPrefix = 'D';
const char kOfflinePageDownloadsPrefix = 'O';

const char* kMaxSuggestionsCountParamName = "downloads_max_count";

// Maximal number of dismissed asset download IDs stored at any time.
const int kMaxDismissedIdCount = 100;

bool CompareOfflinePagesMostRecentlyCreatedFirst(const OfflinePageItem& left,
                                                 const OfflinePageItem& right) {
  return left.creation_time > right.creation_time;
}

int GetMaxSuggestionsCount() {
  bool assets_enabled =
      base::FeatureList::IsEnabled(features::kAssetDownloadSuggestionsFeature);
  return variations::GetVariationParamByFeatureAsInt(
      assets_enabled ? features::kAssetDownloadSuggestionsFeature
                     : features::kOfflinePageDownloadSuggestionsFeature,
      kMaxSuggestionsCountParamName, kDefaultMaxSuggestionsCount);
}

std::string GetOfflinePagePerCategoryID(int64_t raw_offline_page_id) {
  // Raw ID is prefixed in order to avoid conflicts with asset downloads.
  return std::string(1, kOfflinePageDownloadsPrefix) +
         base::IntToString(raw_offline_page_id);
}

std::string GetAssetDownloadPerCategoryID(uint32_t raw_download_id) {
  // Raw ID is prefixed in order to avoid conflicts with offline page downloads.
  return std::string(1, kAssetDownloadsPrefix) +
         base::UintToString(raw_download_id);
}

// Determines whether |suggestion_id| corresponds to offline page suggestion or
// asset download based on |id_within_category| prefix.
bool CorrespondsToOfflinePage(const ContentSuggestion::ID& suggestion_id) {
  const std::string& id_within_category = suggestion_id.id_within_category();
  if (!id_within_category.empty()) {
    if (id_within_category[0] == kOfflinePageDownloadsPrefix) {
      return true;
    }
    if (id_within_category[0] == kAssetDownloadsPrefix) {
      return false;
    }
  }
  NOTREACHED() << "Unknown id_within_category " << id_within_category;
  return false;
}

bool IsDownloadCompleted(const DownloadItem& item) {
  return item.GetState() == DownloadItem::DownloadState::COMPLETE &&
         !item.GetFileExternallyRemoved();
}

base::Time GetAssetDownloadPublishedTime(const DownloadItem& item) {
  return item.GetStartTime();
}

bool CompareDownloadsMostRecentlyDownloadedFirst(const DownloadItem* left,
                                                 const DownloadItem* right) {
  return GetAssetDownloadPublishedTime(*left) >
         GetAssetDownloadPublishedTime(*right);
}

bool IsClientIdForOfflinePageDownload(
    offline_pages::ClientPolicyController* policy_controller,
    const offline_pages::ClientId& client_id) {
  return policy_controller->IsSupportedByDownload(client_id.name_space);
}

std::unique_ptr<OfflinePageModelQuery> BuildOfflinePageDownloadsQuery(
    offline_pages::OfflinePageModel* model) {
  OfflinePageModelQueryBuilder builder;
  builder.RequireSupportedByDownload(
      OfflinePageModelQuery::Requirement::INCLUDE_MATCHING);
  return builder.Build(model->GetPolicyController());
}

}  // namespace

DownloadSuggestionsProvider::DownloadSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    offline_pages::OfflinePageModel* offline_page_model,
    content::DownloadManager* download_manager,
    PrefService* pref_service,
    bool download_manager_ui_enabled)
    : ContentSuggestionsProvider(observer),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(Category::FromKnownCategory(
          ntp_snippets::KnownCategories::DOWNLOADS)),
      offline_page_model_(offline_page_model),
      download_manager_(download_manager),
      pref_service_(pref_service),
      download_manager_ui_enabled_(download_manager_ui_enabled),
      weak_ptr_factory_(this) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);

  DCHECK(offline_page_model_ || download_manager_);
  if (offline_page_model_) {
    offline_page_model_->AddObserver(this);
  }

  if (download_manager_) {
    download_manager_->AddObserver(this);
  }

  // We explicitly fetch the asset downloads in case some of |OnDownloadCreated|
  // happened earlier and, therefore, were missed.
  AsynchronouslyFetchAllDownloadsAndSubmitSuggestions();
}

DownloadSuggestionsProvider::~DownloadSuggestionsProvider() {
  if (offline_page_model_) {
    offline_page_model_->RemoveObserver(this);
  }

  if (download_manager_) {
    download_manager_->RemoveObserver(this);
    UnregisterDownloadItemObservers();
  }
}

CategoryStatus DownloadSuggestionsProvider::GetCategoryStatus(
    Category category) {
  DCHECK_EQ(provided_category_, category);
  return category_status_;
}

CategoryInfo DownloadSuggestionsProvider::GetCategoryInfo(Category category) {
  DCHECK_EQ(provided_category_, category);
  return CategoryInfo(
      l10n_util::GetStringUTF16(IDS_NTP_DOWNLOAD_SUGGESTIONS_SECTION_HEADER),
      ntp_snippets::ContentSuggestionsCardLayout::MINIMAL_CARD,
      /*has_more_action=*/false,
      /*has_reload_action=*/false,
      /*has_view_all_action=*/download_manager_ui_enabled_,
      /*show_if_empty=*/false,
      l10n_util::GetStringUTF16(IDS_NTP_DOWNLOADS_SUGGESTIONS_SECTION_EMPTY));
}

void DownloadSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  DCHECK_EQ(provided_category_, suggestion_id.category());

  AddToDismissedStorageIfNeeded(suggestion_id);
  RemoveSuggestionFromCacheAndRetrieveMoreIfNeeded(suggestion_id);
}

void DownloadSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ntp_snippets::ImageFetchedCallback& callback) {
  // TODO(vitaliii): Fetch proper thumbnail from OfflinePageModel once it is
  // available there.
  // TODO(vitaliii): Provide site's favicon for assets downloads or file type.
  // See crbug.com/631447.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, gfx::Image()));
}

void DownloadSuggestionsProvider::Fetch(
    const ntp_snippets::Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const ntp_snippets::FetchDoneCallback& callback) {
  LOG(DFATAL) << "DownloadSuggestionsProvider has no |Fetch| functionality!";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          callback,
          ntp_snippets::Status(
              ntp_snippets::StatusCode::PERMANENT_ERROR,
              "DownloadSuggestionsProvider has no |Fetch| functionality!"),
          base::Passed(std::vector<ContentSuggestion>())));
}

void DownloadSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  cached_offline_page_downloads_.clear();
  cached_asset_downloads_.clear();
  // This will trigger an asynchronous re-fetch.
  ClearDismissedSuggestionsForDebugging(provided_category_);
}

void DownloadSuggestionsProvider::ClearCachedSuggestions(Category category) {
  DCHECK_EQ(provided_category_, category);
  // Ignored. The internal caches are not stored on disk and they are just
  // partial copies of the data stored at OfflinePage model and DownloadManager.
  // If it is cleared there, it will be cleared in these caches as well.
}

void DownloadSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const ntp_snippets::DismissedSuggestionsCallback& callback) {
  DCHECK_EQ(provided_category_, category);

  if (offline_page_model_) {
    // Offline pages which are not related to downloads are also queried here,
    // so that they can be returned if they happen to be dismissed (e.g. due to
    // a bug).
    OfflinePageModelQueryBuilder query_builder;
    offline_page_model_->GetPagesMatchingQuery(
        query_builder.Build(offline_page_model_->GetPolicyController()),
        base::Bind(&DownloadSuggestionsProvider::
                       GetPagesMatchingQueryCallbackForGetDismissedSuggestions,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  } else {
    GetPagesMatchingQueryCallbackForGetDismissedSuggestions(
        callback, std::vector<OfflinePageItem>());
  }
}

void DownloadSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(provided_category_, category);
  StoreAssetDismissedIDsToPrefs(std::vector<std::string>());
  StoreOfflinePageDismissedIDsToPrefs(std::set<std::string>());
  AsynchronouslyFetchAllDownloadsAndSubmitSuggestions();
}

// static
void DownloadSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDismissedAssetDownloadSuggestions);
  registry->RegisterListPref(kDismissedOfflinePageDownloadSuggestions);
}

int DownloadSuggestionsProvider::GetMaxDismissedCountForTesting() {
  return kMaxDismissedIdCount;
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void DownloadSuggestionsProvider::
    GetPagesMatchingQueryCallbackForGetDismissedSuggestions(
        const ntp_snippets::DismissedSuggestionsCallback& callback,
        const std::vector<OfflinePageItem>& offline_pages) const {
  std::set<std::string> dismissed_offline_ids =
      ReadOfflinePageDismissedIDsFromPrefs();
  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem& item : offline_pages) {
    if (dismissed_offline_ids.count(
            GetOfflinePagePerCategoryID(item.offline_id))) {
      suggestions.push_back(ConvertOfflinePage(item));
    }
  }

  if (download_manager_) {
    std::vector<DownloadItem*> all_downloads;
    download_manager_->GetAllDownloads(&all_downloads);

    std::vector<std::string> dismissed_asset_ids =
        ReadAssetDismissedIDsFromPrefs();

    for (const DownloadItem* item : all_downloads) {
      if (ContainsValue(dismissed_asset_ids,
                        GetAssetDownloadPerCategoryID(item->GetId()))) {
        suggestions.push_back(ConvertDownloadItem(*item));
      }
    }
  }

  callback.Run(std::move(suggestions));
}

void DownloadSuggestionsProvider::OfflinePageModelLoaded(
    offline_pages::OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  AsynchronouslyFetchOfflinePagesDownloads(/*notify=*/true);
}

void DownloadSuggestionsProvider::OfflinePageAdded(
    offline_pages::OfflinePageModel* model,
    const offline_pages::OfflinePageItem& added_page) {
  DCHECK_EQ(offline_page_model_, model);
  if (!IsClientIdForOfflinePageDownload(model->GetPolicyController(),
                                        added_page.client_id)) {
    return;
  }

  // This is all in one statement so that it is completely compiled out in
  // release builds.
  DCHECK_EQ(ReadOfflinePageDismissedIDsFromPrefs().count(
                GetOfflinePagePerCategoryID(added_page.offline_id)),
            0U);

  int max_suggestions_count = GetMaxSuggestionsCount();
  if (static_cast<int>(cached_offline_page_downloads_.size()) <
      max_suggestions_count) {
    cached_offline_page_downloads_.push_back(added_page);
  } else if (max_suggestions_count > 0) {
    auto oldest_page_iterator =
        std::max_element(cached_offline_page_downloads_.begin(),
                         cached_offline_page_downloads_.end(),
                         &CompareOfflinePagesMostRecentlyCreatedFirst);
    *oldest_page_iterator = added_page;
  }

  SubmitContentSuggestions();
}

void DownloadSuggestionsProvider::OfflinePageDeleted(
    int64_t offline_id,
    const offline_pages::ClientId& client_id) {
  DCHECK(offline_page_model_);
  if (IsClientIdForOfflinePageDownload(
          offline_page_model_->GetPolicyController(), client_id)) {
    InvalidateSuggestion(GetOfflinePagePerCategoryID(offline_id));
  }
}

void DownloadSuggestionsProvider::OnDownloadCreated(DownloadManager* manager,
                                                    DownloadItem* item) {
  DCHECK_EQ(download_manager_, manager);
  // This is called when new downloads are started and on startup for existing
  // ones. We listen to each item to know when it is destroyed.
  item->AddObserver(this);
  if (CacheAssetDownloadIfNeeded(item)) {
    SubmitContentSuggestions();
  }
}

void DownloadSuggestionsProvider::ManagerGoingDown(DownloadManager* manager) {
  DCHECK_EQ(download_manager_, manager);
  UnregisterDownloadItemObservers();
  download_manager_ = nullptr;
}

void DownloadSuggestionsProvider::OnDownloadUpdated(DownloadItem* item) {
  if (ContainsValue(cached_asset_downloads_, item)) {
    if (item->GetFileExternallyRemoved()) {
      InvalidateSuggestion(GetAssetDownloadPerCategoryID(item->GetId()));
    } else {
      // The download may have changed.
      SubmitContentSuggestions();
    }
  } else {
    // Unfinished downloads may become completed.
    if (CacheAssetDownloadIfNeeded(item)) {
      SubmitContentSuggestions();
    }
  }
}

void DownloadSuggestionsProvider::OnDownloadOpened(DownloadItem* item) {
  // Ignored.
}

void DownloadSuggestionsProvider::OnDownloadRemoved(DownloadItem* item) {
  // Ignored. We listen to |OnDownloadDestroyed| instead. The reason is that
  // we may need to retrieve all downloads, but |OnDownloadRemoved| is called
  // before the download is removed from the list.
}

void DownloadSuggestionsProvider::OnDownloadDestroyed(
    content::DownloadItem* item) {
  item->RemoveObserver(this);

  if (!IsDownloadCompleted(*item)) {
    return;
  }
  // TODO(vitaliii): Implement a better way to clean up dismissed IDs (in case
  // some calls are missed).
  InvalidateSuggestion(GetAssetDownloadPerCategoryID(item->GetId()));
}

void DownloadSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  DCHECK_NE(CategoryStatus::NOT_PROVIDED, category_status_);
  DCHECK_NE(CategoryStatus::NOT_PROVIDED, new_status);
  if (category_status_ == new_status) {
    return;
  }
  category_status_ = new_status;
  observer()->OnCategoryStatusChanged(this, provided_category_,
                                      category_status_);
}

void DownloadSuggestionsProvider::AsynchronouslyFetchOfflinePagesDownloads(
    bool notify) {
  if (!offline_page_model_) {
    // Offline pages are explicitly turned off, so we propagate "no pages"
    // further e.g. to clean its prefs.
    UpdateOfflinePagesCache(notify, std::vector<OfflinePageItem>());
    return;
  }

  if (!offline_page_model_->is_loaded()) {
    // Offline pages model is not ready yet and may return no offline pages.
    if (notify) {
      SubmitContentSuggestions();
    }

    return;
  }

  offline_page_model_->GetPagesMatchingQuery(
      BuildOfflinePageDownloadsQuery(offline_page_model_),
      base::Bind(&DownloadSuggestionsProvider::UpdateOfflinePagesCache,
                 weak_ptr_factory_.GetWeakPtr(), notify));
}

void DownloadSuggestionsProvider::FetchAssetsDownloads() {
  if (!download_manager_) {
    // The manager has gone down or was explicitly turned off.
    return;
  }

  std::vector<DownloadItem*> all_downloads;
  download_manager_->GetAllDownloads(&all_downloads);
  std::vector<std::string> old_dismissed_ids = ReadAssetDismissedIDsFromPrefs();
  cached_asset_downloads_.clear();
  for (const DownloadItem* item : all_downloads) {
    std::string within_category_id =
        GetAssetDownloadPerCategoryID(item->GetId());
    if (!ContainsValue(old_dismissed_ids, within_category_id)) {
      if (IsDownloadCompleted(*item)) {
        cached_asset_downloads_.push_back(item);
      }
    }
  }

  // We do not prune dismissed IDs, because it is not possible to ensure that
  // the list of downloads is complete (i.e. DownloadManager has finished
  // reading them).
  // TODO(vitaliii): Prune dismissed IDs once the |OnLoaded| notification is
  // provided. See crbug.com/672758.
  const int max_suggestions_count = GetMaxSuggestionsCount();
  if (static_cast<int>(cached_asset_downloads_.size()) >
      max_suggestions_count) {
    // Partially sorts |downloads| such that:
    // 1) The element at the index |max_suggestions_count| is changed to the
    //    element which would occur on this position if |downloads| was sorted;
    // 2) All of the elements before index |max_suggestions_count| are less than
    //    or equal to the elements after it.
    std::nth_element(cached_asset_downloads_.begin(),
                     cached_asset_downloads_.begin() + max_suggestions_count,
                     cached_asset_downloads_.end(),
                     &CompareDownloadsMostRecentlyDownloadedFirst);
    cached_asset_downloads_.resize(max_suggestions_count);
  }
}

void DownloadSuggestionsProvider::
    AsynchronouslyFetchAllDownloadsAndSubmitSuggestions() {
  FetchAssetsDownloads();
  AsynchronouslyFetchOfflinePagesDownloads(/*notify=*/true);
}

void DownloadSuggestionsProvider::SubmitContentSuggestions() {
  NotifyStatusChanged(CategoryStatus::AVAILABLE);

  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem& item : cached_offline_page_downloads_) {
    suggestions.push_back(ConvertOfflinePage(item));
  }

  for (const DownloadItem* item : cached_asset_downloads_) {
    suggestions.push_back(ConvertDownloadItem(*item));
  }

  std::sort(suggestions.begin(), suggestions.end(),
            [](const ContentSuggestion& left, const ContentSuggestion& right) {
              return left.publish_date() > right.publish_date();
            });

  const int max_suggestions_count = GetMaxSuggestionsCount();
  if (static_cast<int>(suggestions.size()) > max_suggestions_count) {
    suggestions.erase(suggestions.begin() + max_suggestions_count,
                      suggestions.end());
  }

  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

ContentSuggestion DownloadSuggestionsProvider::ConvertOfflinePage(
    const OfflinePageItem& offline_page) const {
  ContentSuggestion suggestion(
      ContentSuggestion::ID(provided_category_, GetOfflinePagePerCategoryID(
                                                    offline_page.offline_id)),
      offline_page.url);

  if (offline_page.title.empty()) {
    // TODO(vitaliii): Remove this fallback once the OfflinePageModel provides
    // titles for all (relevant) OfflinePageItems.
    suggestion.set_title(base::UTF8ToUTF16(offline_page.url.spec()));
  } else {
    suggestion.set_title(offline_page.title);
  }
  suggestion.set_publish_date(offline_page.creation_time);
  suggestion.set_publisher_name(base::UTF8ToUTF16(offline_page.url.host()));
  auto extra = base::MakeUnique<ntp_snippets::DownloadSuggestionExtra>();
  extra->is_download_asset = false;
  extra->offline_page_id = offline_page.offline_id;
  suggestion.set_download_suggestion_extra(std::move(extra));
  return suggestion;
}

ContentSuggestion DownloadSuggestionsProvider::ConvertDownloadItem(
    const DownloadItem& download_item) const {
  ContentSuggestion suggestion(
      ContentSuggestion::ID(provided_category_, GetAssetDownloadPerCategoryID(
                                                    download_item.GetId())),
      download_item.GetOriginalUrl());
  suggestion.set_title(
      download_item.GetTargetFilePath().BaseName().LossyDisplayName());
  suggestion.set_publish_date(GetAssetDownloadPublishedTime(download_item));
  suggestion.set_publisher_name(
      base::UTF8ToUTF16(download_item.GetURL().host()));
  auto extra = base::MakeUnique<ntp_snippets::DownloadSuggestionExtra>();
  extra->target_file_path = download_item.GetTargetFilePath();
  extra->mime_type = download_item.GetMimeType();
  extra->is_download_asset = true;
  suggestion.set_download_suggestion_extra(std::move(extra));
  return suggestion;
}

bool DownloadSuggestionsProvider::CacheAssetDownloadIfNeeded(
    const content::DownloadItem* item) {
  if (!IsDownloadCompleted(*item)) {
    return false;
  }

  if (ContainsValue(cached_asset_downloads_, item)) {
    return false;
  }

  std::vector<std::string> dismissed_ids = ReadAssetDismissedIDsFromPrefs();
  if (ContainsValue(dismissed_ids,
                    GetAssetDownloadPerCategoryID(item->GetId()))) {
    return false;
  }

  DCHECK_LE(static_cast<int>(cached_asset_downloads_.size()),
            GetMaxSuggestionsCount());
  if (static_cast<int>(cached_asset_downloads_.size()) ==
      GetMaxSuggestionsCount()) {
    auto oldest = std::max_element(
        cached_asset_downloads_.begin(), cached_asset_downloads_.end(),
        &CompareDownloadsMostRecentlyDownloadedFirst);
    if (GetAssetDownloadPublishedTime(*item) <=
        GetAssetDownloadPublishedTime(**oldest)) {
      return false;
    }

    *oldest = item;
  } else {
    cached_asset_downloads_.push_back(item);
  }

  return true;
}

bool DownloadSuggestionsProvider::RemoveSuggestionFromCacheIfPresent(
    const ContentSuggestion::ID& suggestion_id) {
  DCHECK_EQ(provided_category_, suggestion_id.category());
  if (CorrespondsToOfflinePage(suggestion_id)) {
    auto matching =
        std::find_if(cached_offline_page_downloads_.begin(),
                     cached_offline_page_downloads_.end(),
                     [&suggestion_id](const OfflinePageItem& item) {
                       return GetOfflinePagePerCategoryID(item.offline_id) ==
                              suggestion_id.id_within_category();
                     });
    if (matching != cached_offline_page_downloads_.end()) {
      cached_offline_page_downloads_.erase(matching);
      return true;
    }
    return false;
  }

  auto matching = std::find_if(
      cached_asset_downloads_.begin(), cached_asset_downloads_.end(),
      [&suggestion_id](const DownloadItem* item) {
        return GetAssetDownloadPerCategoryID(item->GetId()) ==
               suggestion_id.id_within_category();
      });
  if (matching != cached_asset_downloads_.end()) {
    cached_asset_downloads_.erase(matching);
    return true;
  }
  return false;
}

void DownloadSuggestionsProvider::
    RemoveSuggestionFromCacheAndRetrieveMoreIfNeeded(
        const ContentSuggestion::ID& suggestion_id) {
  DCHECK_EQ(provided_category_, suggestion_id.category());
  if (!RemoveSuggestionFromCacheIfPresent(suggestion_id)) {
    return;
  }

  if (CorrespondsToOfflinePage(suggestion_id)) {
    if (static_cast<int>(cached_offline_page_downloads_.size()) ==
        GetMaxSuggestionsCount() - 1) {
      // Previously there were |GetMaxSuggestionsCount()| cached suggestion,
      // therefore, overall there may be more than |GetMaxSuggestionsCount()|
      // suggestions in the model and now one of them may be cached instead of
      // the removed one. Even though, the suggestions are not immediately
      // used the cache has to be kept up to date, because it may be used when
      // other data source is updated.
      AsynchronouslyFetchOfflinePagesDownloads(/*notify=*/false);
    }
  } else {
    if (static_cast<int>(cached_asset_downloads_.size()) ==
        GetMaxSuggestionsCount() - 1) {
      // The same as the case above.
      FetchAssetsDownloads();
    }
  }
}

void DownloadSuggestionsProvider::UpdateOfflinePagesCache(
    bool notify,
    const std::vector<offline_pages::OfflinePageItem>&
        all_download_offline_pages) {
  DCHECK(!offline_page_model_ || offline_page_model_->is_loaded());

  std::set<std::string> old_dismissed_ids =
      ReadOfflinePageDismissedIDsFromPrefs();
  std::set<std::string> retained_dismissed_ids;
  std::vector<const OfflinePageItem*> items;
  // Filtering out dismissed items and pruning dismissed IDs.
  for (const OfflinePageItem& item : all_download_offline_pages) {
    std::string id_within_category =
        GetOfflinePagePerCategoryID(item.offline_id);
    if (!old_dismissed_ids.count(id_within_category)) {
      items.push_back(&item);
    } else {
      retained_dismissed_ids.insert(id_within_category);
    }
  }

  const int max_suggestions_count = GetMaxSuggestionsCount();
  if (static_cast<int>(items.size()) > max_suggestions_count) {
    // Partially sorts |items| such that:
    // 1) The element at the index |max_suggestions_count| is changed to the
    //    element which would occur on this position if |items| was sorted;
    // 2) All of the elements before index |max_suggestions_count| are less than
    //    or equal to the elements after it.
    std::nth_element(
        items.begin(), items.begin() + max_suggestions_count, items.end(),
        [](const OfflinePageItem* left, const OfflinePageItem* right) {
          return CompareOfflinePagesMostRecentlyCreatedFirst(*left, *right);
        });
    items.resize(max_suggestions_count);
  }

  cached_offline_page_downloads_.clear();
  for (const OfflinePageItem* item : items) {
    cached_offline_page_downloads_.push_back(*item);
  }

  if (old_dismissed_ids.size() != retained_dismissed_ids.size()) {
    StoreOfflinePageDismissedIDsToPrefs(retained_dismissed_ids);
  }

  if (notify) {
    SubmitContentSuggestions();
  }
}

void DownloadSuggestionsProvider::InvalidateSuggestion(
    const std::string& id_within_category) {
  ContentSuggestion::ID suggestion_id(provided_category_, id_within_category);
  observer()->OnSuggestionInvalidated(this, suggestion_id);

  RemoveFromDismissedStorageIfNeeded(suggestion_id);
  RemoveSuggestionFromCacheAndRetrieveMoreIfNeeded(suggestion_id);
}

// TODO(vitaliii): Do not use std::vector, when we ensure that pruning happens
// at the right time (crbug.com/672758).
std::vector<std::string>
DownloadSuggestionsProvider::ReadAssetDismissedIDsFromPrefs() const {
  std::vector<std::string> dismissed_ids;
  const base::ListValue* list =
      pref_service_->GetList(kDismissedAssetDownloadSuggestions);
  for (const std::unique_ptr<base::Value>& value : *list) {
    std::string dismissed_id;
    bool success = value->GetAsString(&dismissed_id);
    DCHECK(success) << "Failed to parse dismissed id from prefs param "
                    << kDismissedAssetDownloadSuggestions << " into string.";
    dismissed_ids.push_back(dismissed_id);
  }
  return dismissed_ids;
}

void DownloadSuggestionsProvider::StoreAssetDismissedIDsToPrefs(
    const std::vector<std::string>& dismissed_ids) {
  DCHECK(std::all_of(
      dismissed_ids.begin(), dismissed_ids.end(),
      [](const std::string& id) { return id[0] == kAssetDownloadsPrefix; }));

  base::ListValue list;
  for (const std::string& dismissed_id : dismissed_ids) {
    list.AppendString(dismissed_id);
  }
  pref_service_->Set(kDismissedAssetDownloadSuggestions, list);
}

std::set<std::string>
DownloadSuggestionsProvider::ReadOfflinePageDismissedIDsFromPrefs() const {
  return ntp_snippets::prefs::ReadDismissedIDsFromPrefs(
      *pref_service_, kDismissedOfflinePageDownloadSuggestions);
}

void DownloadSuggestionsProvider::StoreOfflinePageDismissedIDsToPrefs(
    const std::set<std::string>& dismissed_ids) {
  DCHECK(std::all_of(dismissed_ids.begin(), dismissed_ids.end(),
                     [](const std::string& id) {
                       return id[0] == kOfflinePageDownloadsPrefix;
                     }));
  ntp_snippets::prefs::StoreDismissedIDsToPrefs(
      pref_service_, kDismissedOfflinePageDownloadSuggestions, dismissed_ids);
}

void DownloadSuggestionsProvider::AddToDismissedStorageIfNeeded(
    const ContentSuggestion::ID& suggestion_id) {
  if (CorrespondsToOfflinePage(suggestion_id)) {
    std::set<std::string> dismissed_ids =
        ReadOfflinePageDismissedIDsFromPrefs();
    dismissed_ids.insert(suggestion_id.id_within_category());
    StoreOfflinePageDismissedIDsToPrefs(dismissed_ids);
  } else {
    std::vector<std::string> dismissed_ids = ReadAssetDismissedIDsFromPrefs();
    // The suggestion may be double dismissed from previously opened NTPs.
    if (!ContainsValue(dismissed_ids, suggestion_id.id_within_category())) {
      dismissed_ids.push_back(suggestion_id.id_within_category());
      // TODO(vitaliii): Remove this workaround once the dismissed ids are
      // pruned. See crbug.com/672758.
      while (dismissed_ids.size() > kMaxDismissedIdCount) {
        dismissed_ids.erase(dismissed_ids.begin());
      }
      StoreAssetDismissedIDsToPrefs(dismissed_ids);
    }
  }
}

void DownloadSuggestionsProvider::RemoveFromDismissedStorageIfNeeded(
    const ContentSuggestion::ID& suggestion_id) {
  if (CorrespondsToOfflinePage(suggestion_id)) {
    std::set<std::string> dismissed_ids =
        ReadOfflinePageDismissedIDsFromPrefs();
    if (dismissed_ids.count(suggestion_id.id_within_category())) {
      dismissed_ids.erase(suggestion_id.id_within_category());
      StoreOfflinePageDismissedIDsToPrefs(dismissed_ids);
    }
  } else {
    std::vector<std::string> dismissed_ids = ReadAssetDismissedIDsFromPrefs();
    auto it = std::find(dismissed_ids.begin(), dismissed_ids.end(),
                        suggestion_id.id_within_category());
    if (it != dismissed_ids.end()) {
      dismissed_ids.erase(it);
      StoreAssetDismissedIDsToPrefs(dismissed_ids);
    }
  }
}

void DownloadSuggestionsProvider::UnregisterDownloadItemObservers() {
  DCHECK_NE(download_manager_, nullptr);

  std::vector<DownloadItem*> all_downloads;
  download_manager_->GetAllDownloads(&all_downloads);

  for (DownloadItem* item : all_downloads) {
    item->RemoveObserver(this);
  }
}

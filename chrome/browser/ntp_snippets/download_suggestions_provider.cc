// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/download_suggestions_provider.h"

#include <algorithm>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/pref_util.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using content::DownloadItem;
using content::DownloadManager;
using ntp_snippets::Category;
using ntp_snippets::CategoryInfo;
using ntp_snippets::CategoryStatus;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::prefs::kDismissedAssetDownloadSuggestions;
using ntp_snippets::prefs::kDismissedOfflinePageDownloadSuggestions;
using offline_pages::OfflinePageItem;

namespace {

// TODO(vitaliii): Make this configurable via a variation param. See
// crbug.com/654800.
const int kMaxSuggestionsCount = 5;
const char kAssetDownloadsPrefix = 'D';
const char kOfflinePageDownloadsPrefix = 'O';

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
    if (id_within_category[0] == kOfflinePageDownloadsPrefix)
      return true;
    if (id_within_category[0] == kAssetDownloadsPrefix)
      return false;
  }
  NOTREACHED() << "Unknown id_within_category " << id_within_category;
  return false;
}

bool IsOfflinePageDownload(const offline_pages::ClientId& client_id) {
  return client_id.name_space == offline_pages::kAsyncNamespace ||
         client_id.name_space == offline_pages::kDownloadNamespace ||
         client_id.name_space == offline_pages::kNTPSuggestionsNamespace;
}

bool IsDownloadCompleted(const DownloadItem& item) {
  return item.GetState() == DownloadItem::DownloadState::COMPLETE &&
         !item.GetFileExternallyRemoved();
}

struct OrderDownloadsMostRecentlyDownloadedFirst {
  bool operator()(const DownloadItem* left, const DownloadItem* right) const {
    return left->GetEndTime() > right->GetEndTime();
  }
};

}  // namespace

DownloadSuggestionsProvider::DownloadSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    ntp_snippets::CategoryFactory* category_factory,
    scoped_refptr<ntp_snippets::OfflinePageProxy> offline_page_proxy,
    content::DownloadManager* download_manager,
    PrefService* pref_service,
    bool download_manager_ui_enabled)
    : ContentSuggestionsProvider(observer, category_factory),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(category_factory->FromKnownCategory(
          ntp_snippets::KnownCategories::DOWNLOADS)),
      offline_page_proxy_(std::move(offline_page_proxy)),
      download_manager_(download_manager),
      pref_service_(pref_service),
      download_manager_ui_enabled_(download_manager_ui_enabled),
      weak_ptr_factory_(this) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);
  offline_page_proxy_->AddObserver(this);
  if (download_manager_)
    download_manager_->AddObserver(this);
  // No need to explicitly fetch the asset downloads, since for each of them
  // |OnDownloadCreated| is fired.
  AsynchronouslyFetchOfflinePagesDownloads(/*notify=*/true);
}

DownloadSuggestionsProvider::~DownloadSuggestionsProvider() {
  offline_page_proxy_->RemoveObserver(this);
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
  // TODO(vitaliii): Do not show "More" button when there is no downloads UI.
  // See crbug.com/660030.
  return CategoryInfo(
      l10n_util::GetStringUTF16(IDS_NTP_DOWNLOAD_SUGGESTIONS_SECTION_HEADER),
      ntp_snippets::ContentSuggestionsCardLayout::MINIMAL_CARD,
      /*has_more_button=*/download_manager_ui_enabled_,
      /*show_if_empty=*/false,
      l10n_util::GetStringUTF16(IDS_NTP_DOWNLOADS_SUGGESTIONS_SECTION_EMPTY));
}

void DownloadSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  DCHECK_EQ(provided_category_, suggestion_id.category());
  std::set<std::string> dismissed_ids =
      ReadDismissedIDsFromPrefs(CorrespondsToOfflinePage(suggestion_id));
  dismissed_ids.insert(suggestion_id.id_within_category());
  StoreDismissedIDsToPrefs(CorrespondsToOfflinePage(suggestion_id),
                           dismissed_ids);

  RemoveSuggestionFromCacheAndRetrieveMoreIfNeeded(suggestion_id);
}

void DownloadSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(vitaliii): Fetch proper thumbnail from OfflinePageModel once it is
  // available there.
  // TODO(vitaliii): Provide site's favicon for assets downloads. See
  // crbug.com/631447.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, gfx::Image()));
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
    const DismissedSuggestionsCallback& callback) {
  DCHECK_EQ(provided_category_, category);

  offline_page_proxy_->GetAllPages(
      base::Bind(&DownloadSuggestionsProvider::
                     GetAllPagesCallbackForGetDismissedSuggestions,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void DownloadSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(provided_category_, category);
  StoreAssetDismissedIDsToPrefs(std::set<std::string>());
  StoreOfflinePageDismissedIDsToPrefs(std::set<std::string>());
  AsynchronouslyFetchAllDownloadsAndSubmitSuggestions();
}

// static
void DownloadSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDismissedAssetDownloadSuggestions);
  registry->RegisterListPref(kDismissedOfflinePageDownloadSuggestions);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void DownloadSuggestionsProvider::GetAllPagesCallbackForGetDismissedSuggestions(
    const DismissedSuggestionsCallback& callback,
    const std::vector<OfflinePageItem>& offline_pages) const {
  std::set<std::string> dismissed_ids = ReadOfflinePageDismissedIDsFromPrefs();
  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem& item : offline_pages) {
    if (dismissed_ids.count(GetOfflinePagePerCategoryID(item.offline_id)))
      suggestions.push_back(ConvertOfflinePage(item));
  }

  if (download_manager_) {
    std::vector<DownloadItem*> all_downloads;
    download_manager_->GetAllDownloads(&all_downloads);

    dismissed_ids = ReadAssetDismissedIDsFromPrefs();

    for (const DownloadItem* item : all_downloads) {
      if (dismissed_ids.count(GetAssetDownloadPerCategoryID(item->GetId())))
        suggestions.push_back(ConvertDownloadItem(*item));
    }
  }

  callback.Run(std::move(suggestions));
}

void DownloadSuggestionsProvider::OfflinePageModelChanged(
    const std::vector<offline_pages::OfflinePageItem>& offline_pages) {
  UpdateOfflinePagesCache(/*notify=*/true, offline_pages);
}

void DownloadSuggestionsProvider::OfflinePageDeleted(
    int64_t offline_id,
    const offline_pages::ClientId& client_id) {
  if (IsOfflinePageDownload(client_id))
    InvalidateSuggestion(GetOfflinePagePerCategoryID(offline_id));
}

void DownloadSuggestionsProvider::OnDownloadCreated(DownloadManager* manager,
                                                    DownloadItem* item) {
  DCHECK_EQ(download_manager_, manager);
  // This is called when new downloads are started and on startup for existing
  // ones. We listen to each item to know when it is destroyed.
  item->AddObserver(this);
  if (CacheAssetDownloadIfNeeded(item))
    SubmitContentSuggestions();
}

void DownloadSuggestionsProvider::ManagerGoingDown(DownloadManager* manager) {
  DCHECK_EQ(download_manager_, manager);
  UnregisterDownloadItemObservers();
  download_manager_ = nullptr;
}

void DownloadSuggestionsProvider::OnDownloadUpdated(DownloadItem* item) {
  if (base::ContainsValue(cached_asset_downloads_, item)) {
    if (item->GetFileExternallyRemoved()) {
      InvalidateSuggestion(GetAssetDownloadPerCategoryID(item->GetId()));
    } else {
      // The download may have changed.
      SubmitContentSuggestions();
    }
  } else {
    // Unfinished downloads may become completed.
    if (CacheAssetDownloadIfNeeded(item))
      SubmitContentSuggestions();
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

  if (!IsDownloadCompleted(*item))
    return;
  // TODO(vitaliii): Implement a better way to clean up dismissed IDs (in case
  // some calls are missed).
  InvalidateSuggestion(GetAssetDownloadPerCategoryID(item->GetId()));
}

void DownloadSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  DCHECK_NE(CategoryStatus::NOT_PROVIDED, category_status_);
  DCHECK_NE(CategoryStatus::NOT_PROVIDED, new_status);
  if (category_status_ == new_status)
    return;
  category_status_ = new_status;
  observer()->OnCategoryStatusChanged(this, provided_category_,
                                      category_status_);
}

void DownloadSuggestionsProvider::AsynchronouslyFetchOfflinePagesDownloads(
    bool notify) {
  offline_page_proxy_->GetAllPages(
      base::Bind(&DownloadSuggestionsProvider::UpdateOfflinePagesCache,
                 weak_ptr_factory_.GetWeakPtr(), notify));
}

void DownloadSuggestionsProvider::FetchAssetsDownloads() {
  if (!download_manager_) {
    // The manager has gone down.
    return;
  }

  std::vector<DownloadItem*> all_downloads;
  download_manager_->GetAllDownloads(&all_downloads);
  std::set<std::string> old_dismissed_ids = ReadAssetDismissedIDsFromPrefs();
  std::set<std::string> retained_dismissed_ids;
  cached_asset_downloads_.clear();
  for (const DownloadItem* item : all_downloads) {
    std::string within_category_id =
        GetAssetDownloadPerCategoryID(item->GetId());
    if (!old_dismissed_ids.count(within_category_id)) {
      if (IsDownloadCompleted(*item))
        cached_asset_downloads_.push_back(item);
    } else {
      retained_dismissed_ids.insert(within_category_id);
    }
  }

  if (old_dismissed_ids.size() != retained_dismissed_ids.size())
    StoreAssetDismissedIDsToPrefs(retained_dismissed_ids);

  if (static_cast<int>(cached_asset_downloads_.size()) > kMaxSuggestionsCount) {
    // Partially sorts |downloads| such that:
    // 1) The element at the index |kMaxSuggestionsCount| is changed to the
    //    element which would occur on this position if |downloads| was sorted;
    // 2) All of the elements before index |kMaxSuggestionsCount| are less than
    //    or equal to the elements after it.
    std::nth_element(cached_asset_downloads_.begin(),
                     cached_asset_downloads_.begin() + kMaxSuggestionsCount,
                     cached_asset_downloads_.end(),
                     OrderDownloadsMostRecentlyDownloadedFirst());
    cached_asset_downloads_.resize(kMaxSuggestionsCount);
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
  for (const OfflinePageItem& item : cached_offline_page_downloads_)
    suggestions.push_back(ConvertOfflinePage(item));

  for (const DownloadItem* item : cached_asset_downloads_)
    suggestions.push_back(ConvertDownloadItem(*item));

  std::sort(suggestions.begin(), suggestions.end(),
            [](const ContentSuggestion& left, const ContentSuggestion& right) {
              return left.publish_date() > right.publish_date();
            });

  // TODO(vitaliii): Use resize() here. In order to do so, mark
  // ContentSuggestion move constructor noexcept.
  while (suggestions.size() > kMaxSuggestionsCount)
    suggestions.pop_back();

  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

ContentSuggestion DownloadSuggestionsProvider::ConvertOfflinePage(
    const OfflinePageItem& offline_page) const {
  // TODO(vitaliii): Make sure the URL is actually opened as an offline URL even
  // when the user is online. See crbug.com/641568.
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
  return suggestion;
}

ContentSuggestion DownloadSuggestionsProvider::ConvertDownloadItem(
    const DownloadItem& download_item) const {
  // TODO(vitaliii): Ensure that files are opened in browser, but not downloaded
  // again. See crbug.com/641568.
  ContentSuggestion suggestion(
      ContentSuggestion::ID(provided_category_, GetAssetDownloadPerCategoryID(
                                                    download_item.GetId())),
      net::FilePathToFileURL(download_item.GetTargetFilePath()));
  // TODO(vitaliii): Set proper title.
  suggestion.set_title(
      download_item.GetTargetFilePath().BaseName().LossyDisplayName());
  suggestion.set_publish_date(download_item.GetEndTime());
  suggestion.set_publisher_name(
      base::UTF8ToUTF16(download_item.GetURL().host()));
  // TODO(vitaliii): Set suggestion icon.
  return suggestion;
}

bool DownloadSuggestionsProvider::CacheAssetDownloadIfNeeded(
    const content::DownloadItem* item) {
  if (!IsDownloadCompleted(*item))
    return false;

  if (base::ContainsValue(cached_asset_downloads_, item))
    return false;

  std::set<std::string> dismissed_ids = ReadAssetDismissedIDsFromPrefs();
  if (dismissed_ids.count(GetAssetDownloadPerCategoryID(item->GetId())))
    return false;

  DCHECK_LE(static_cast<int>(cached_asset_downloads_.size()),
            kMaxSuggestionsCount);
  if (cached_asset_downloads_.size() == kMaxSuggestionsCount) {
    auto oldest = std::max_element(cached_asset_downloads_.begin(),
                                   cached_asset_downloads_.end(),
                                   OrderDownloadsMostRecentlyDownloadedFirst());
    if (item->GetEndTime() <= (*oldest)->GetEndTime())
      return false;

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
  if (!RemoveSuggestionFromCacheIfPresent(suggestion_id))
    return;

  if (CorrespondsToOfflinePage(suggestion_id)) {
    if (cached_offline_page_downloads_.size() == kMaxSuggestionsCount - 1) {
      // Previously there were |kMaxSuggestionsCount| cached suggestion,
      // therefore, overall there may be more than |kMaxSuggestionsCount|
      // suggestions in the model and now one of them may be cached instead of
      // the removed one. Even though, the suggestions are not immediately
      // used the cache has to be kept up to date, because it may be used when
      // other data source is updated.
      AsynchronouslyFetchOfflinePagesDownloads(/*notify=*/false);
    }
  } else {
    if (cached_asset_downloads_.size() == kMaxSuggestionsCount - 1) {
      // The same as the case above.
      FetchAssetsDownloads();
    }
  }
}

void DownloadSuggestionsProvider::UpdateOfflinePagesCache(
    bool notify,
    const std::vector<offline_pages::OfflinePageItem>& all_offline_pages) {
  std::set<std::string> old_dismissed_ids =
      ReadOfflinePageDismissedIDsFromPrefs();
  std::set<std::string> retained_dismissed_ids;
  std::vector<const OfflinePageItem*> items;
  // Filtering out dismissed items and pruning dismissed IDs.
  for (const OfflinePageItem& item : all_offline_pages) {
    if (!IsOfflinePageDownload(item.client_id))
      continue;

    std::string id_within_category =
        GetOfflinePagePerCategoryID(item.offline_id);
    if (!old_dismissed_ids.count(id_within_category))
      items.push_back(&item);
    else
      retained_dismissed_ids.insert(id_within_category);
  }

  if (static_cast<int>(items.size()) > kMaxSuggestionsCount) {
    // Partially sorts |items| such that:
    // 1) The element at the index |kMaxSuggestionsCount| is changed to the
    //    element which would occur on this position if |items| was sorted;
    // 2) All of the elements before index |kMaxSuggestionsCount| are less than
    //    or equal to the elements after it.
    std::nth_element(
        items.begin(), items.begin() + kMaxSuggestionsCount, items.end(),
        [](const OfflinePageItem* left, const OfflinePageItem* right) {
          return left->creation_time > right->creation_time;
        });
    items.resize(kMaxSuggestionsCount);
  }

  cached_offline_page_downloads_.clear();
  for (const OfflinePageItem* item : items)
    cached_offline_page_downloads_.push_back(*item);

  if (old_dismissed_ids.size() != retained_dismissed_ids.size())
    StoreOfflinePageDismissedIDsToPrefs(retained_dismissed_ids);

  if (notify)
    SubmitContentSuggestions();
}

void DownloadSuggestionsProvider::InvalidateSuggestion(
    const std::string& id_within_category) {
  ContentSuggestion::ID suggestion_id(provided_category_, id_within_category);
  observer()->OnSuggestionInvalidated(this, suggestion_id);

  std::set<std::string> dismissed_ids =
      ReadDismissedIDsFromPrefs(CorrespondsToOfflinePage(suggestion_id));
  auto it = dismissed_ids.find(id_within_category);
  if (it != dismissed_ids.end()) {
    dismissed_ids.erase(it);
    StoreDismissedIDsToPrefs(CorrespondsToOfflinePage(suggestion_id),
                             dismissed_ids);
  }

  RemoveSuggestionFromCacheAndRetrieveMoreIfNeeded(suggestion_id);
}

std::set<std::string>
DownloadSuggestionsProvider::ReadAssetDismissedIDsFromPrefs() const {
  return ntp_snippets::prefs::ReadDismissedIDsFromPrefs(
      *pref_service_, kDismissedAssetDownloadSuggestions);
}

void DownloadSuggestionsProvider::StoreAssetDismissedIDsToPrefs(
    const std::set<std::string>& dismissed_ids) {
  DCHECK(std::all_of(
      dismissed_ids.begin(), dismissed_ids.end(),
      [](const std::string& id) { return id[0] == kAssetDownloadsPrefix; }));
  ntp_snippets::prefs::StoreDismissedIDsToPrefs(
      pref_service_, kDismissedAssetDownloadSuggestions, dismissed_ids);
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

std::set<std::string> DownloadSuggestionsProvider::ReadDismissedIDsFromPrefs(
    bool for_offline_page_downloads) const {
  if (for_offline_page_downloads)
    return ReadOfflinePageDismissedIDsFromPrefs();
  return ReadAssetDismissedIDsFromPrefs();
}

// TODO(vitaliii): Store one set instead of two. See crbug.com/656024.
void DownloadSuggestionsProvider::StoreDismissedIDsToPrefs(
    bool for_offline_page_downloads,
    const std::set<std::string>& dismissed_ids) {
  if (for_offline_page_downloads)
    StoreOfflinePageDismissedIDsToPrefs(dismissed_ids);
  else
    StoreAssetDismissedIDsToPrefs(dismissed_ids);
}

void DownloadSuggestionsProvider::UnregisterDownloadItemObservers() {
  DCHECK_NE(download_manager_, nullptr);

  std::vector<DownloadItem*> all_downloads;
  download_manager_->GetAllDownloads(&all_downloads);

  for (DownloadItem* item : all_downloads)
    item->RemoveObserver(this);
}

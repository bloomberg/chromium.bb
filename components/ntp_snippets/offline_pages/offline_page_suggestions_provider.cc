// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/offline_page_suggestions_provider.h"

#include <algorithm>

#include "base/bind.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using offline_pages::MultipleOfflinePageItemResult;
using offline_pages::OfflinePageModel;
using offline_pages::OfflinePageItem;

namespace ntp_snippets {

namespace {

const int kMaxSuggestionsCount = 5;

struct OrderByMostRecentlyVisited {
  bool operator()(const OfflinePageItem* left,
                  const OfflinePageItem* right) const {
    return left->last_access_time > right->last_access_time;
  }
};

bool IsRecentTab(const offline_pages::ClientId& client_id) {
  return client_id.name_space == offline_pages::kLastNNamespace;
}

bool IsDownload(const offline_pages::ClientId& client_id) {
  // TODO(pke): Use kDownloadNamespace once the OfflinePageModel uses that.
  // The current logic is taken from DownloadUIAdapter::IsVisibleInUI.
  return client_id.name_space == offline_pages::kAsyncNamespace &&
         base::IsValidGUID(client_id.id);
}

}  // namespace

OfflinePageSuggestionsProvider::OfflinePageSuggestionsProvider(
    bool recent_tabs_enabled,
    bool downloads_enabled,
    bool download_manager_ui_enabled,
    ContentSuggestionsProvider::Observer* observer,
    CategoryFactory* category_factory,
    OfflinePageModel* offline_page_model,
    PrefService* pref_service)
    : ContentSuggestionsProvider(observer, category_factory),
      recent_tabs_status_(recent_tabs_enabled
                              ? CategoryStatus::AVAILABLE_LOADING
                              : CategoryStatus::NOT_PROVIDED),
      downloads_status_(downloads_enabled ? CategoryStatus::AVAILABLE_LOADING
                                          : CategoryStatus::NOT_PROVIDED),
      offline_page_model_(offline_page_model),
      recent_tabs_category_(
          category_factory->FromKnownCategory(KnownCategories::RECENT_TABS)),
      downloads_category_(
          category_factory->FromKnownCategory(KnownCategories::DOWNLOADS)),
      pref_service_(pref_service),
      download_manager_ui_enabled_(download_manager_ui_enabled) {
  DCHECK(recent_tabs_enabled || downloads_enabled);
  if (recent_tabs_enabled) {
    observer->OnCategoryStatusChanged(this, recent_tabs_category_,
                                      recent_tabs_status_);
  }
  if (downloads_enabled) {
    observer->OnCategoryStatusChanged(this, downloads_category_,
                                      downloads_status_);
  }
  offline_page_model_->AddObserver(this);
  FetchOfflinePages();
}

OfflinePageSuggestionsProvider::~OfflinePageSuggestionsProvider() {
  offline_page_model_->RemoveObserver(this);
}

// static
void OfflinePageSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDismissedRecentOfflineTabSuggestions);
  registry->RegisterListPref(prefs::kDismissedDownloadSuggestions);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

CategoryStatus OfflinePageSuggestionsProvider::GetCategoryStatus(
    Category category) {
  if (category == recent_tabs_category_)
    return recent_tabs_status_;
  if (category == downloads_category_)
    return downloads_status_;
  NOTREACHED() << "Unknown category " << category.id();
  return CategoryStatus::NOT_PROVIDED;
}

CategoryInfo OfflinePageSuggestionsProvider::GetCategoryInfo(
    Category category) {
  if (category == recent_tabs_category_) {
    return CategoryInfo(l10n_util::GetStringUTF16(
                            IDS_NTP_RECENT_TAB_SUGGESTIONS_SECTION_HEADER),
                        ContentSuggestionsCardLayout::MINIMAL_CARD,
                        /* has_more_button */ false,
                        /* show_if_empty */ false);
  }
  if (category == downloads_category_) {
    return CategoryInfo(
        l10n_util::GetStringUTF16(IDS_NTP_DOWNLOAD_SUGGESTIONS_SECTION_HEADER),
        ContentSuggestionsCardLayout::MINIMAL_CARD,
        /* has_more_button */ download_manager_ui_enabled_,
        /* show_if_empty */ false);
  }
  NOTREACHED() << "Unknown category " << category.id();
  return CategoryInfo(base::string16(),
                      ContentSuggestionsCardLayout::MINIMAL_CARD,
                      /* has_more_button */ false,
                      /* show_if_empty */ false);
}

void OfflinePageSuggestionsProvider::DismissSuggestion(
    const std::string& suggestion_id) {
  Category category = GetCategoryFromUniqueID(suggestion_id);
  std::string offline_page_id = GetWithinCategoryIDFromUniqueID(suggestion_id);
  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs(category);
  dismissed_ids.insert(offline_page_id);
  StoreDismissedIDsToPrefs(category, dismissed_ids);
}

void OfflinePageSuggestionsProvider::FetchSuggestionImage(
    const std::string& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(pke): Fetch proper thumbnail from OfflinePageModel once it's available
  // there.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, gfx::Image()));
}

void OfflinePageSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  // TODO(vitaliii): Implement. See crbug.com/641321.
}

void OfflinePageSuggestionsProvider::ClearCachedSuggestions(Category category) {
  // Ignored.
}

void OfflinePageSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  offline_page_model_->GetAllPages(
      base::Bind(&OfflinePageSuggestionsProvider::
                     OnOfflinePagesLoadedForDismissedDebugging,
                 base::Unretained(this), category, callback));
}

void OfflinePageSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  StoreDismissedIDsToPrefs(category, std::set<std::string>());
  FetchOfflinePages();
}

void OfflinePageSuggestionsProvider::OfflinePageModelLoaded(
    OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
}

void OfflinePageSuggestionsProvider::OfflinePageModelChanged(
    OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  FetchOfflinePages();
}

void OfflinePageSuggestionsProvider::OfflinePageDeleted(
    int64_t offline_id,
    const offline_pages::ClientId& client_id) {
  // Because we never switch to NOT_PROVIDED dynamically, there can be no open
  // UI containing an invalidated suggestion unless the status is something
  // other than NOT_PROVIDED, so only notify invalidation in that case.
  if (recent_tabs_status_ != CategoryStatus::NOT_PROVIDED &&
      IsRecentTab(client_id)) {
    InvalidateSuggestion(recent_tabs_category_, offline_id);
  } else if (downloads_status_ != CategoryStatus::NOT_PROVIDED &&
             IsDownload(client_id)) {
    InvalidateSuggestion(downloads_category_, offline_id);
  }
}

void OfflinePageSuggestionsProvider::FetchOfflinePages() {
  // TODO(pke): When something other than GetAllPages is used here, the
  // dismissed IDs cleanup in OnOfflinePagesLoaded needs to be changed to avoid
  // suggestions being undismissed accidentally.
  offline_page_model_->GetAllPages(
      base::Bind(&OfflinePageSuggestionsProvider::OnOfflinePagesLoaded,
                 base::Unretained(this)));
}

void OfflinePageSuggestionsProvider::OnOfflinePagesLoaded(
    const MultipleOfflinePageItemResult& result) {
  bool need_recent_tabs = recent_tabs_status_ != CategoryStatus::NOT_PROVIDED;
  bool need_downloads = downloads_status_ != CategoryStatus::NOT_PROVIDED;
  if (need_recent_tabs)
    NotifyStatusChanged(recent_tabs_category_, CategoryStatus::AVAILABLE);
  if (need_downloads)
    NotifyStatusChanged(downloads_category_, CategoryStatus::AVAILABLE);

  std::set<std::string> dismissed_recent_tab_ids =
      ReadDismissedIDsFromPrefs(recent_tabs_category_);
  std::set<std::string> dismissed_download_ids =
      ReadDismissedIDsFromPrefs(downloads_category_);
  std::set<std::string> cleaned_recent_tab_ids;
  std::set<std::string> cleaned_download_ids;
  std::vector<const OfflinePageItem*> recent_tab_items;
  std::vector<const OfflinePageItem*> download_items;
  for (const OfflinePageItem& item : result) {
    std::string offline_page_id = base::IntToString(item.offline_id);
    if (need_recent_tabs && IsRecentTab(item.client_id)) {
      if (dismissed_recent_tab_ids.count(offline_page_id))
        cleaned_recent_tab_ids.insert(offline_page_id);
      else
        recent_tab_items.push_back(&item);
    }

    if (need_downloads && IsDownload(item.client_id)) {
      if (dismissed_download_ids.count(offline_page_id))
        cleaned_download_ids.insert(offline_page_id);
      else
        download_items.push_back(&item);
    }
  }

  // TODO(pke): Once we have our OfflinePageModel getter and that doesn't do it
  // already, filter out duplicate URLs for recent tabs here. Duplicates for
  // downloads are fine.

  if (need_recent_tabs) {
    observer()->OnNewSuggestions(
        this, recent_tabs_category_,
        GetMostRecentlyVisited(recent_tabs_category_,
                               std::move(recent_tab_items)));
    if (cleaned_recent_tab_ids.size() != dismissed_recent_tab_ids.size())
      StoreDismissedIDsToPrefs(recent_tabs_category_, cleaned_recent_tab_ids);
  }
  if (need_downloads) {
    observer()->OnNewSuggestions(
        this, downloads_category_,
        GetMostRecentlyVisited(downloads_category_, std::move(download_items)));
    if (cleaned_download_ids.size() != dismissed_download_ids.size())
      StoreDismissedIDsToPrefs(downloads_category_, cleaned_download_ids);
  }
}

void OfflinePageSuggestionsProvider::OnOfflinePagesLoadedForDismissedDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback,
    const offline_pages::MultipleOfflinePageItemResult& result) {
  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs(category);
  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem& item : result) {
    if (category == recent_tabs_category_ && !IsRecentTab(item.client_id))
      continue;
    if (category == downloads_category_ && !IsDownload(item.client_id))
      continue;
    if (!dismissed_ids.count(base::IntToString(item.offline_id)))
      continue;
    suggestions.push_back(ConvertOfflinePage(category, item));
  }
  callback.Run(std::move(suggestions));
}

void OfflinePageSuggestionsProvider::NotifyStatusChanged(
    Category category,
    CategoryStatus new_status) {
  if (category == recent_tabs_category_) {
    DCHECK_NE(CategoryStatus::NOT_PROVIDED, recent_tabs_status_);
    if (recent_tabs_status_ == new_status)
      return;
    recent_tabs_status_ = new_status;
    observer()->OnCategoryStatusChanged(this, category, new_status);
  } else if (category == downloads_category_) {
    DCHECK_NE(CategoryStatus::NOT_PROVIDED, downloads_status_);
    if (downloads_status_ == new_status)
      return;
    downloads_status_ = new_status;
    observer()->OnCategoryStatusChanged(this, category, new_status);
  } else {
    NOTREACHED() << "Unknown category " << category.id();
  }
}

ContentSuggestion OfflinePageSuggestionsProvider::ConvertOfflinePage(
    Category category,
    const OfflinePageItem& offline_page) const {
  // TODO(pke): Make sure the URL is actually opened as an offline URL.
  // Currently, the browser opens the offline URL and then immediately
  // redirects to the online URL if the device is online.
  ContentSuggestion suggestion(
      MakeUniqueID(category, base::IntToString(offline_page.offline_id)),
      offline_page.GetOfflineURL());

  if (offline_page.title.empty()) {
    // TODO(pke): Remove this fallback once the OfflinePageModel provides titles
    // for all (relevant) OfflinePageItems.
    suggestion.set_title(base::UTF8ToUTF16(offline_page.url.spec()));
  } else {
    suggestion.set_title(offline_page.title);
  }
  suggestion.set_publish_date(offline_page.creation_time);
  suggestion.set_publisher_name(base::UTF8ToUTF16(offline_page.url.host()));
  return suggestion;
}

std::vector<ContentSuggestion>
OfflinePageSuggestionsProvider::GetMostRecentlyVisited(
    Category category,
    std::vector<const OfflinePageItem*> offline_page_items) const {
  std::sort(offline_page_items.begin(), offline_page_items.end(),
            OrderByMostRecentlyVisited());
  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem* offline_page_item : offline_page_items) {
    suggestions.push_back(ConvertOfflinePage(category, *offline_page_item));
    if (suggestions.size() == kMaxSuggestionsCount)
      break;
  }
  return suggestions;
}

void OfflinePageSuggestionsProvider::InvalidateSuggestion(Category category,
                                                          int64_t offline_id) {
  std::string offline_page_id = base::IntToString(offline_id);
  observer()->OnSuggestionInvalidated(this, category,
                                      MakeUniqueID(category, offline_page_id));

  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs(category);
  auto it =
      std::find(dismissed_ids.begin(), dismissed_ids.end(), offline_page_id);
  if (it != dismissed_ids.end()) {
    dismissed_ids.erase(it);
    StoreDismissedIDsToPrefs(category, dismissed_ids);
  }
}

std::string OfflinePageSuggestionsProvider::GetDismissedPref(
    Category category) const {
  if (category == recent_tabs_category_)
    return prefs::kDismissedRecentOfflineTabSuggestions;
  if (category == downloads_category_)
    return prefs::kDismissedDownloadSuggestions;
  NOTREACHED() << "Unknown category " << category.id();
  return std::string();
}

std::set<std::string> OfflinePageSuggestionsProvider::ReadDismissedIDsFromPrefs(
    Category category) const {
  std::set<std::string> dismissed_ids;
  const base::ListValue* list =
      pref_service_->GetList(GetDismissedPref(category));
  for (const std::unique_ptr<base::Value>& value : *list) {
    std::string dismissed_id;
    bool success = value->GetAsString(&dismissed_id);
    DCHECK(success) << "Failed to parse dismissed offline page ID from prefs";
    dismissed_ids.insert(dismissed_id);
  }
  return dismissed_ids;
}

void OfflinePageSuggestionsProvider::StoreDismissedIDsToPrefs(
    Category category,
    const std::set<std::string>& dismissed_ids) {
  base::ListValue list;
  for (const std::string& dismissed_id : dismissed_ids)
    list.AppendString(dismissed_id);
  pref_service_->Set(GetDismissedPref(category), list);
}

}  // namespace ntp_snippets

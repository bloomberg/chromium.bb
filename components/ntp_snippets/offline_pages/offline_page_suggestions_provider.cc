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
      dismissed_recent_tab_ids_(ReadDismissedIDsFromPrefs(
          prefs::kDismissedRecentOfflineTabSuggestions)),
      dismissed_download_ids_(
          ReadDismissedIDsFromPrefs(prefs::kDismissedDownloadSuggestions)),
      download_manager_ui_enabled_(download_manager_ui_enabled) {
  DCHECK(recent_tabs_enabled || downloads_enabled);
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

std::vector<Category> OfflinePageSuggestionsProvider::GetProvidedCategories() {
  std::vector<Category> provided_categories;
  if (recent_tabs_status_ != CategoryStatus::NOT_PROVIDED)
    provided_categories.push_back(recent_tabs_category_);
  if (downloads_status_ != CategoryStatus::NOT_PROVIDED)
    provided_categories.push_back(downloads_category_);
  return provided_categories;
}

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
                        /* has_more_button */ false);
  }
  if (category == downloads_category_) {
    return CategoryInfo(
        l10n_util::GetStringUTF16(IDS_NTP_DOWNLOAD_SUGGESTIONS_SECTION_HEADER),
        ContentSuggestionsCardLayout::MINIMAL_CARD,
        /* has_more_button */ download_manager_ui_enabled_);
  }
  NOTREACHED() << "Unknown category " << category.id();
  return CategoryInfo(base::string16(),
                      ContentSuggestionsCardLayout::MINIMAL_CARD,
                      /* has_more_button */ false);
}

void OfflinePageSuggestionsProvider::DismissSuggestion(
    const std::string& suggestion_id) {
  Category category = GetCategoryFromUniqueID(suggestion_id);
  std::string offline_page_id = GetWithinCategoryIDFromUniqueID(suggestion_id);
  if (category == recent_tabs_category_) {
    DCHECK_NE(CategoryStatus::NOT_PROVIDED, recent_tabs_status_);
    dismissed_recent_tab_ids_.insert(offline_page_id);
    StoreDismissedIDsToPrefs(prefs::kDismissedRecentOfflineTabSuggestions,
                             dismissed_recent_tab_ids_);
  } else if (category == downloads_category_) {
    DCHECK_NE(CategoryStatus::NOT_PROVIDED, downloads_status_);
    dismissed_download_ids_.insert(offline_page_id);
    StoreDismissedIDsToPrefs(prefs::kDismissedDownloadSuggestions,
                             dismissed_download_ids_);
  } else {
    NOTREACHED() << "Unknown category " << category.id();
  }
}

void OfflinePageSuggestionsProvider::FetchSuggestionImage(
    const std::string& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(pke): Fetch proper thumbnail from OfflinePageModel once it's available
  // there.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, suggestion_id, gfx::Image()));
}

void OfflinePageSuggestionsProvider::ClearCachedSuggestionsForDebugging(
    Category category) {
  // Ignored.
}

std::vector<ContentSuggestion>
OfflinePageSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category) {
  // TODO(pke): Make GetDismissedSuggestionsForDebugging asynchronous so this
  // can return proper values.
  std::vector<ContentSuggestion> suggestions;
  const std::set<std::string>* dismissed_ids = nullptr;
  if (category == recent_tabs_category_) {
    DCHECK_NE(CategoryStatus::NOT_PROVIDED, recent_tabs_status_);
    dismissed_ids = &dismissed_recent_tab_ids_;
  } else if (category == downloads_category_) {
    DCHECK_NE(CategoryStatus::NOT_PROVIDED, downloads_status_);
    dismissed_ids = &dismissed_download_ids_;
  } else {
    NOTREACHED() << "Unknown category " << category.id();
    return suggestions;
  }

  for (const std::string& dismissed_id : *dismissed_ids) {
    ContentSuggestion suggestion(
        MakeUniqueID(category, dismissed_id),
        GURL("http://dismissed-offline-page-" + dismissed_id));
    suggestion.set_title(base::UTF8ToUTF16("Title not available"));
    suggestions.push_back(std::move(suggestion));
  }
  return suggestions;
}

void OfflinePageSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  if (category == recent_tabs_category_) {
    DCHECK_NE(CategoryStatus::NOT_PROVIDED, recent_tabs_status_);
    dismissed_recent_tab_ids_.clear();
    StoreDismissedIDsToPrefs(prefs::kDismissedRecentOfflineTabSuggestions,
                             dismissed_recent_tab_ids_);
  } else if (category == downloads_category_) {
    DCHECK_NE(CategoryStatus::NOT_PROVIDED, downloads_status_);
    dismissed_download_ids_.clear();
    StoreDismissedIDsToPrefs(prefs::kDismissedDownloadSuggestions,
                             dismissed_download_ids_);
  } else {
    NOTREACHED() << "Unknown category " << category.id();
  }
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
  // TODO(pke): Implement, suggestion has to be removed from UI immediately.
}

void OfflinePageSuggestionsProvider::FetchOfflinePages() {
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

  std::vector<const OfflinePageItem*> recent_tab_items;
  std::vector<const OfflinePageItem*> download_items;
  for (const OfflinePageItem& item : result) {
    if (need_recent_tabs &&
        item.client_id.name_space == offline_pages::kLastNNamespace &&
        !dismissed_recent_tab_ids_.count(base::IntToString(item.offline_id))) {
      recent_tab_items.push_back(&item);
    }

    // TODO(pke): Use kDownloadNamespace once the OfflinePageModel uses that.
    // The current logic is taken from DownloadUIAdapter::IsVisibleInUI.
    if (need_downloads &&
        item.client_id.name_space == offline_pages::kAsyncNamespace &&
        base::IsValidGUID(item.client_id.id) &&
        !dismissed_download_ids_.count(base::IntToString(item.offline_id))) {
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
  }
  if (need_downloads) {
    observer()->OnNewSuggestions(
        this, downloads_category_,
        GetMostRecentlyVisited(downloads_category_, std::move(download_items)));
  }
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

std::set<std::string> OfflinePageSuggestionsProvider::ReadDismissedIDsFromPrefs(
    const std::string& pref_name) const {
  std::set<std::string> dismissed_ids;
  const base::ListValue* list = pref_service_->GetList(pref_name);
  for (const std::unique_ptr<base::Value>& value : *list) {
    std::string dismissed_id;
    bool success = value->GetAsString(&dismissed_id);
    DCHECK(success) << "Failed to parse dismissed offline page ID from prefs";
    dismissed_ids.insert(dismissed_id);
  }
  return dismissed_ids;
}

void OfflinePageSuggestionsProvider::StoreDismissedIDsToPrefs(
    const std::string& pref_name,
    const std::set<std::string>& dismissed_ids) {
  base::ListValue list;
  for (const std::string& dismissed_id : dismissed_ids)
    list.AppendString(dismissed_id);
  pref_service_->Set(pref_name, list);
}

}  // namespace ntp_snippets

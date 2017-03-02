// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/recent_tab_suggestions_provider.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/pref_util.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/recent_tabs/recent_tabs_ui_adapter_delegate.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using offline_pages::ClientId;
using offline_pages::DownloadUIAdapter;
using offline_pages::DownloadUIItem;

namespace ntp_snippets {

namespace {

const int kDefaultMaxSuggestionsCount = 5;

const char* kMaxSuggestionsCountParamName = "recent_tabs_max_count";

int GetMaxSuggestionsCount() {
  return variations::GetVariationParamByFeatureAsInt(
      kRecentOfflineTabSuggestionsFeature, kMaxSuggestionsCountParamName,
      kDefaultMaxSuggestionsCount);
}

struct OrderUIItemsByMostRecentlyCreatedFirst {
  bool operator()(const DownloadUIItem* left,
                  const DownloadUIItem* right) const {
    return left->start_time > right->start_time;
  }
};

struct OrderUIItemsByUrlAndThenMostRecentlyCreatedFirst {
  bool operator()(const DownloadUIItem* left,
                  const DownloadUIItem* right) const {
    if (left->url != right->url) {
      return left->url < right->url;
    }
    return left->start_time > right->start_time;
  }
};

}  // namespace

RecentTabSuggestionsProvider::RecentTabSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    offline_pages::DownloadUIAdapter* ui_adapter,
    PrefService* pref_service)
    : ContentSuggestionsProvider(observer),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(
          Category::FromKnownCategory(KnownCategories::RECENT_TABS)),
      recent_tabs_ui_adapter_(ui_adapter),
      pref_service_(pref_service),
      weak_ptr_factory_(this) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);
  recent_tabs_ui_adapter_->AddObserver(this);
}

RecentTabSuggestionsProvider::~RecentTabSuggestionsProvider() {
  recent_tabs_ui_adapter_->RemoveObserver(this);
}

CategoryStatus RecentTabSuggestionsProvider::GetCategoryStatus(
    Category category) {
  if (category == provided_category_) {
    return category_status_;
  }
  NOTREACHED() << "Unknown category " << category.id();
  return CategoryStatus::NOT_PROVIDED;
}

CategoryInfo RecentTabSuggestionsProvider::GetCategoryInfo(Category category) {
  DCHECK_EQ(provided_category_, category);
  return CategoryInfo(
      l10n_util::GetStringUTF16(IDS_NTP_RECENT_TAB_SUGGESTIONS_SECTION_HEADER),
      ContentSuggestionsCardLayout::MINIMAL_CARD,
      /*has_fetch_action=*/false,
      /*has_view_all_action=*/false,
      /*show_if_empty=*/false,
      l10n_util::GetStringUTF16(IDS_NTP_RECENT_TAB_SUGGESTIONS_SECTION_EMPTY));
}

void RecentTabSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  DCHECK_EQ(provided_category_, suggestion_id.category());
  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  dismissed_ids.insert(suggestion_id.id_within_category());
  StoreDismissedIDsToPrefs(dismissed_ids);
}

void RecentTabSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(vitaliii): Fetch proper thumbnail from OfflinePageModel once it's
  // available there.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, gfx::Image()));
}

void RecentTabSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  LOG(DFATAL) << "RecentTabSuggestionsProvider has no |Fetch| functionality!";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          callback,
          Status(StatusCode::PERMANENT_ERROR,
                 "RecentTabSuggestionsProvider has no |Fetch| functionality!"),
          base::Passed(std::vector<ContentSuggestion>())));
}

void RecentTabSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  ClearDismissedSuggestionsForDebugging(provided_category_);
  FetchRecentTabs();
}

void RecentTabSuggestionsProvider::ClearCachedSuggestions(Category category) {
  // Ignored.
}

void RecentTabSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  DCHECK_EQ(provided_category_, category);

  std::vector<const DownloadUIItem*> items =
      recent_tabs_ui_adapter_->GetAllItems();

  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  std::vector<ContentSuggestion> suggestions;
  for (const DownloadUIItem* item : items) {
    int64_t offline_page_id =
        recent_tabs_ui_adapter_->GetOfflineIdByGuid(item->guid);
    if (!dismissed_ids.count(base::IntToString(offline_page_id))) {
      continue;
    }

    suggestions.push_back(ConvertUIItem(*item));
  }
  callback.Run(std::move(suggestions));
}

void RecentTabSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(provided_category_, category);
  StoreDismissedIDsToPrefs(std::set<std::string>());
  FetchRecentTabs();
}

// static
void RecentTabSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDismissedRecentOfflineTabSuggestions);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void RecentTabSuggestionsProvider::ItemsLoaded() {
  FetchRecentTabs();
}

void RecentTabSuggestionsProvider::ItemAdded(const DownloadUIItem& ui_item) {
  FetchRecentTabs();
}

void RecentTabSuggestionsProvider::ItemUpdated(const DownloadUIItem& ui_item) {
  FetchRecentTabs();
}

void RecentTabSuggestionsProvider::ItemDeleted(
    const std::string& ui_item_guid) {
  // Because we never switch to NOT_PROVIDED dynamically, there can be no open
  // UI containing an invalidated suggestion unless the status is something
  // other than NOT_PROVIDED, so only notify invalidation in that case.
  if (category_status_ != CategoryStatus::NOT_PROVIDED) {
    InvalidateSuggestion(ui_item_guid);
  }
}

void RecentTabSuggestionsProvider::FetchRecentTabs() {
  std::vector<const DownloadUIItem*> ui_items =
      recent_tabs_ui_adapter_->GetAllItems();
  NotifyStatusChanged(CategoryStatus::AVAILABLE);
  std::set<std::string> old_dismissed_ids = ReadDismissedIDsFromPrefs();
  std::set<std::string> new_dismissed_ids;
  std::vector<const DownloadUIItem*> non_dismissed_items;

  for (const DownloadUIItem* item : ui_items) {
    std::string offline_page_id = base::IntToString(
        recent_tabs_ui_adapter_->GetOfflineIdByGuid(item->guid));
    if (old_dismissed_ids.count(offline_page_id)) {
      new_dismissed_ids.insert(offline_page_id);
    } else {
      non_dismissed_items.push_back(item);
    }
  }

  observer()->OnNewSuggestions(
      this, provided_category_,
      GetMostRecentlyCreatedWithoutDuplicates(std::move(non_dismissed_items)));
  if (new_dismissed_ids.size() != old_dismissed_ids.size()) {
    StoreDismissedIDsToPrefs(new_dismissed_ids);
  }
}

void RecentTabSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  DCHECK_NE(CategoryStatus::NOT_PROVIDED, category_status_);
  if (category_status_ == new_status) {
    return;
  }
  category_status_ = new_status;
  observer()->OnCategoryStatusChanged(this, provided_category_, new_status);
}

ContentSuggestion RecentTabSuggestionsProvider::ConvertUIItem(
    const DownloadUIItem& ui_item) const {
  // UI items have the Tab ID embedded in the GUID and the offline ID is
  // available by querying.
  int64_t offline_page_id =
      recent_tabs_ui_adapter_->GetOfflineIdByGuid(ui_item.guid);
  ContentSuggestion suggestion(provided_category_,
                               base::IntToString(offline_page_id), ui_item.url);
  suggestion.set_title(ui_item.title);
  suggestion.set_publish_date(ui_item.start_time);
  suggestion.set_publisher_name(base::UTF8ToUTF16(ui_item.url.host()));
  auto extra = base::MakeUnique<RecentTabSuggestionExtra>();
  int tab_id;
  bool success = base::StringToInt(ui_item.guid, &tab_id);
  DCHECK(success);
  extra->tab_id = tab_id;
  extra->offline_page_id = offline_page_id;
  suggestion.set_recent_tab_suggestion_extra(std::move(extra));

  return suggestion;
}

std::vector<ContentSuggestion>
RecentTabSuggestionsProvider::GetMostRecentlyCreatedWithoutDuplicates(
    std::vector<const DownloadUIItem*> ui_items) const {
  // |std::unique| only removes duplicates that immediately follow each other.
  // Thus, first, we have to sort by URL and creation time and only then remove
  // duplicates and sort the remaining items by creation time.
  std::sort(ui_items.begin(), ui_items.end(),
            OrderUIItemsByUrlAndThenMostRecentlyCreatedFirst());
  std::vector<const DownloadUIItem*>::iterator new_end =
      std::unique(ui_items.begin(), ui_items.end(),
                  [](const DownloadUIItem* left, const DownloadUIItem* right) {
                    return left->url == right->url;
                  });
  ui_items.erase(new_end, ui_items.end());
  std::sort(ui_items.begin(), ui_items.end(),
            OrderUIItemsByMostRecentlyCreatedFirst());
  std::vector<ContentSuggestion> suggestions;
  for (const DownloadUIItem* ui_item : ui_items) {
    suggestions.push_back(ConvertUIItem(*ui_item));
    if (static_cast<int>(suggestions.size()) == GetMaxSuggestionsCount()) {
      break;
    }
  }
  return suggestions;
}

void RecentTabSuggestionsProvider::InvalidateSuggestion(
    const std::string& ui_item_guid) {
  std::string offline_page_id = base::IntToString(
      recent_tabs_ui_adapter_->GetOfflineIdByGuid(ui_item_guid));
  observer()->OnSuggestionInvalidated(
      this, ContentSuggestion::ID(provided_category_, offline_page_id));

  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  auto it = dismissed_ids.find(offline_page_id);
  if (it != dismissed_ids.end()) {
    dismissed_ids.erase(it);
    StoreDismissedIDsToPrefs(dismissed_ids);
  }
}

std::set<std::string> RecentTabSuggestionsProvider::ReadDismissedIDsFromPrefs()
    const {
  return prefs::ReadDismissedIDsFromPrefs(
      *pref_service_, prefs::kDismissedRecentOfflineTabSuggestions);
}

void RecentTabSuggestionsProvider::StoreDismissedIDsToPrefs(
    const std::set<std::string>& dismissed_ids) {
  prefs::StoreDismissedIDsToPrefs(pref_service_,
                                  prefs::kDismissedRecentOfflineTabSuggestions,
                                  dismissed_ids);
}

}  // namespace ntp_snippets

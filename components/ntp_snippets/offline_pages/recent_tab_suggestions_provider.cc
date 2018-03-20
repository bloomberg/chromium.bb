// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/recent_tab_suggestions_provider.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
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
  bool operator()(const OfflineItem& left, const OfflineItem& right) const {
    return left.creation_time > right.creation_time;
  }
};

struct OrderUIItemsByUrlAndThenMostRecentlyCreatedFirst {
  bool operator()(const OfflineItem& left, const OfflineItem& right) const {
    if (left.page_url != right.page_url) {
      return left.page_url < right.page_url;
    }
    return left.creation_time > right.creation_time;
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
      ContentSuggestionsAdditionalAction::NONE,
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
    ImageFetchedCallback callback) {
  // TODO(vitaliii): Fetch proper thumbnail from OfflinePageModel once it's
  // available there.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), gfx::Image()));
}

void RecentTabSuggestionsProvider::FetchSuggestionImageData(
    const ContentSuggestion::ID& suggestion_id,
    ImageDataFetchedCallback callback) {
  // TODO(vitaliii): Fetch proper thumbnail from OfflinePageModel once it's
  // available there.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::string()));
}

void RecentTabSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    FetchDoneCallback callback) {
  LOG(DFATAL) << "RecentTabSuggestionsProvider has no |Fetch| functionality!";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          Status(StatusCode::PERMANENT_ERROR,
                 "RecentTabSuggestionsProvider has no |Fetch| functionality!"),
          std::vector<ContentSuggestion>()));
}

void RecentTabSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  ClearDismissedSuggestionsForDebugging(provided_category_);
  FetchRecentTabs();
}

void RecentTabSuggestionsProvider::ClearCachedSuggestions() {
  // Ignored.
}

void RecentTabSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    DismissedSuggestionsCallback callback) {
  DCHECK_EQ(provided_category_, category);
  recent_tabs_ui_adapter_->GetAllItems(base::BindOnce(
      &RecentTabSuggestionsProvider::OnGetDismissedSuggestionsForDebuggingDone,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RecentTabSuggestionsProvider::OnGetDismissedSuggestionsForDebuggingDone(
    DismissedSuggestionsCallback callback,
    const std::vector<OfflineItem>& offline_items) {
  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  std::vector<ContentSuggestion> suggestions;
  for (const OfflineItem& item : offline_items) {
    int64_t offline_page_id =
        recent_tabs_ui_adapter_->GetOfflineIdByGuid(item.id.id);
    if (!dismissed_ids.count(base::IntToString(offline_page_id))) {
      continue;
    }

    suggestions.push_back(ConvertUIItem(item));
  }

  std::move(callback).Run(std::move(suggestions));
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

void RecentTabSuggestionsProvider::OnItemsAdded(
    const std::vector<OfflineItem>& items) {
  FetchRecentTabs();
}

void RecentTabSuggestionsProvider::OnItemUpdated(const OfflineItem& item) {
  FetchRecentTabs();
}

void RecentTabSuggestionsProvider::OnItemRemoved(const ContentId& id) {
  // Because we never switch to NOT_PROVIDED dynamically, there can be no open
  // UI containing an invalidated suggestion unless the status is something
  // other than NOT_PROVIDED, so only notify invalidation in that case.
  if (category_status_ != CategoryStatus::NOT_PROVIDED) {
    InvalidateSuggestion(id.id);
  }
}

void RecentTabSuggestionsProvider::FetchRecentTabs() {
  recent_tabs_ui_adapter_->GetAllItems(
      base::BindOnce(&RecentTabSuggestionsProvider::OnFetchRecentTabsDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RecentTabSuggestionsProvider::OnFetchRecentTabsDone(
    const std::vector<OfflineItem>& offline_items) {
  NotifyStatusChanged(CategoryStatus::AVAILABLE);
  std::set<std::string> old_dismissed_ids = ReadDismissedIDsFromPrefs();
  std::set<std::string> new_dismissed_ids;
  std::vector<OfflineItem> non_dismissed_items;

  for (const OfflineItem& item : offline_items) {
    std::string offline_page_id = base::IntToString(
        recent_tabs_ui_adapter_->GetOfflineIdByGuid(item.id.id));
    if (old_dismissed_ids.count(offline_page_id)) {
      new_dismissed_ids.insert(offline_page_id);
    } else {
      non_dismissed_items.push_back(item);
    }
  }

  observer()->OnNewSuggestions(
      this, provided_category_,
      GetMostRecentlyCreatedWithoutDuplicates(non_dismissed_items));
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
    const OfflineItem& ui_item) const {
  // UI items have the Tab ID embedded in the GUID and the offline ID is
  // available by querying.
  int64_t offline_page_id =
      recent_tabs_ui_adapter_->GetOfflineIdByGuid(ui_item.id.id);
  ContentSuggestion suggestion(
      provided_category_, base::IntToString(offline_page_id), ui_item.page_url);
  suggestion.set_title(base::UTF8ToUTF16(ui_item.title));
  suggestion.set_publish_date(ui_item.creation_time);
  suggestion.set_publisher_name(base::UTF8ToUTF16(ui_item.page_url.host()));
  auto extra = std::make_unique<RecentTabSuggestionExtra>();
  int tab_id;
  bool success = base::StringToInt(ui_item.id.id, &tab_id);
  DCHECK(success);
  extra->tab_id = tab_id;
  extra->offline_page_id = offline_page_id;
  suggestion.set_recent_tab_suggestion_extra(std::move(extra));

  return suggestion;
}

std::vector<ContentSuggestion>
RecentTabSuggestionsProvider::GetMostRecentlyCreatedWithoutDuplicates(
    std::vector<OfflineItem>& ui_items) const {
  // |std::unique| only removes duplicates that immediately follow each other.
  // Thus, first, we have to sort by URL and creation time and only then remove
  // duplicates and sort the remaining items by creation time.
  std::sort(ui_items.begin(), ui_items.end(),
            OrderUIItemsByUrlAndThenMostRecentlyCreatedFirst());
  std::vector<OfflineItem>::iterator new_end =
      std::unique(ui_items.begin(), ui_items.end(),
                  [](const OfflineItem& left, const OfflineItem& right) {
                    return left.page_url == right.page_url;
                  });
  ui_items.erase(new_end, ui_items.end());
  std::sort(ui_items.begin(), ui_items.end(),
            OrderUIItemsByMostRecentlyCreatedFirst());
  std::vector<ContentSuggestion> suggestions;
  for (const OfflineItem& ui_item : ui_items) {
    suggestions.push_back(ConvertUIItem(ui_item));
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

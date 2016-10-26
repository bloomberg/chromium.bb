// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/sessions/foreign_sessions_suggestions_provider.h"

#include <algorithm>
#include <map>
#include <tuple>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/pref_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/synced_session.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using base::TimeDelta;
using sessions::SerializedNavigationEntry;
using sessions::SessionTab;
using sessions::SessionWindow;
using sync_sessions::SyncedSession;

namespace ntp_snippets {
namespace {

const int kMaxForeignTabsTotal = 10;
const int kMaxForeignTabsPerDevice = 3;
const int kMaxForeignTabAgeInMinutes = 180;

const char* kMaxForeignTabsTotalParamName = "max_foreign_tabs_total";
const char* kMaxForeignTabsPerDeviceParamName = "max_foreign_tabs_per_device";
const char* kMaxForeignTabAgeInMinutesParamName =
    "max_foreign_tabs_age_in_minutes";

int GetMaxForeignTabsTotal() {
  return GetParamAsInt(ntp_snippets::kForeignSessionsSuggestionsFeature,
                       kMaxForeignTabsTotalParamName, kMaxForeignTabsTotal);
}

int GetMaxForeignTabsPerDevice() {
  return GetParamAsInt(ntp_snippets::kForeignSessionsSuggestionsFeature,
                       kMaxForeignTabsPerDeviceParamName,
                       kMaxForeignTabsPerDevice);
}

TimeDelta GetMaxForeignTabAge() {
  return TimeDelta::FromMinutes(GetParamAsInt(
      ntp_snippets::kForeignSessionsSuggestionsFeature,
      kMaxForeignTabAgeInMinutesParamName, kMaxForeignTabAgeInMinutes));
}

}  // namespace

// Collection of pointers to various sessions objects that contain a superset of
// the information needed to create a single suggestion.
struct ForeignSessionsSuggestionsProvider::SessionData {
  const sync_sessions::SyncedSession* session;
  const sessions::SessionTab* tab;
  const sessions::SerializedNavigationEntry* navigation;
  bool operator<(const SessionData& other) const {
    // Note that SerializedNavigationEntry::timestamp() is never set to a
    // value, so always use SessionTab::timestamp() instead.
    // TODO(skym): It might be better if we sorted by recency of session, and
    // only then by recency of the tab. Right now this causes a single
    // device's tabs to be interleaved with another devices' tabs.
    return tab->timestamp > other.tab->timestamp;
  }
};

ForeignSessionsSuggestionsProvider::ForeignSessionsSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    CategoryFactory* category_factory,
    std::unique_ptr<ForeignSessionsProvider> foreign_sessions_provider,
    PrefService* pref_service)
    : ContentSuggestionsProvider(observer, category_factory),
      category_status_(CategoryStatus::INITIALIZING),
      provided_category_(
          category_factory->FromKnownCategory(KnownCategories::FOREIGN_TABS)),
      foreign_sessions_provider_(std::move(foreign_sessions_provider)),
      pref_service_(pref_service) {
  foreign_sessions_provider_->SubscribeForForeignTabChange(
      base::Bind(&ForeignSessionsSuggestionsProvider::OnForeignTabChange,
                 base::Unretained(this)));

  // If sync is already initialzed, try suggesting now, though this is unlikely.
  OnForeignTabChange();
}

ForeignSessionsSuggestionsProvider::~ForeignSessionsSuggestionsProvider() =
    default;

// static
void ForeignSessionsSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDismissedForeignSessionsSuggestions);
}

CategoryStatus ForeignSessionsSuggestionsProvider::GetCategoryStatus(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  return category_status_;
}

CategoryInfo ForeignSessionsSuggestionsProvider::GetCategoryInfo(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  return CategoryInfo(
      l10n_util::GetStringUTF16(
          IDS_NTP_FOREIGN_SESSIONS_SUGGESTIONS_SECTION_HEADER),
      ContentSuggestionsCardLayout::MINIMAL_CARD,
      /*has_more_button=*/true,
      /*show_if_empty=*/false,
      l10n_util::GetStringUTF16(IDS_NTP_SUGGESTIONS_SECTION_EMPTY));
  // TODO(skym): Replace IDS_NTP_SUGGESTIONS_SECTION_EMPTY with a
  // category-specific string.
}

void ForeignSessionsSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  // TODO(skym): Right now this continuously grows, without clearing out old and
  // irrelevant entries. Could either use a timestamp and expire after a
  // threshold, or compare with current foreign tabs and remove anything that
  // isn't actively blockign a foreign_sessions tab.
  std::set<std::string> dismissed_ids = prefs::ReadDismissedIDsFromPrefs(
      *pref_service_, prefs::kDismissedForeignSessionsSuggestions);
  dismissed_ids.insert(suggestion_id.id_within_category());
  prefs::StoreDismissedIDsToPrefs(pref_service_,
                                  prefs::kDismissedForeignSessionsSuggestions,
                                  dismissed_ids);
}

void ForeignSessionsSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, gfx::Image()));
}

void ForeignSessionsSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  std::set<std::string> dismissed_ids = prefs::ReadDismissedIDsFromPrefs(
      *pref_service_, prefs::kDismissedForeignSessionsSuggestions);
  for (auto iter = dismissed_ids.begin(); iter != dismissed_ids.end();) {
    if (filter.Run(GURL(*iter))) {
      iter = dismissed_ids.erase(iter);
    } else {
      ++iter;
    }
  }
  prefs::StoreDismissedIDsToPrefs(pref_service_,
                                  prefs::kDismissedForeignSessionsSuggestions,
                                  dismissed_ids);
}

void ForeignSessionsSuggestionsProvider::ClearCachedSuggestions(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  // Ignored.
}

void ForeignSessionsSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  DCHECK_EQ(category, provided_category_);
  callback.Run(std::vector<ContentSuggestion>());
}

void ForeignSessionsSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  pref_service_->ClearPref(prefs::kDismissedForeignSessionsSuggestions);
}

void ForeignSessionsSuggestionsProvider::OnForeignTabChange() {
  if (!foreign_sessions_provider_->HasSessionsData()) {
    if (category_status_ == CategoryStatus::AVAILABLE) {
      // This is to handle the case where the user disabled sync [sessions] or
      // logs out after we've already provided actual suggestions.
      category_status_ = CategoryStatus::NOT_PROVIDED;
      observer()->OnCategoryStatusChanged(this, provided_category_,
                                          category_status_);
    }
    return;
  }

  if (category_status_ != CategoryStatus::AVAILABLE) {
    // The further below logic will overwrite any error state. This is
    // currently okay because no where in the current implementation does the
    // status get set to an error state. Should this change, reconsider the
    // overwriting logic.
    DCHECK(category_status_ == CategoryStatus::INITIALIZING ||
           category_status_ == CategoryStatus::NOT_PROVIDED);

    // It is difficult to tell if sync simply has not initialized yet or there
    // will never be data because the user is signed out or has disabled the
    // sessions data type. Because this provider is hidden when there are no
    // results, always just update to AVAILABLE once we might have results.
    category_status_ = CategoryStatus::AVAILABLE;
    observer()->OnCategoryStatusChanged(this, provided_category_,
                                        category_status_);
  }

  // observer()->OnNewSuggestions must be called even when we have no
  // suggestions to remove previous suggestions that are now filtered out.
  observer()->OnNewSuggestions(this, provided_category_, BuildSuggestions());
}

std::vector<ContentSuggestion>
ForeignSessionsSuggestionsProvider::BuildSuggestions() {
  const int max_foreign_tabs_total = GetMaxForeignTabsTotal();
  const int max_foreign_tabs_per_device = GetMaxForeignTabsPerDevice();

  std::vector<SessionData> suggestion_candidates = GetSuggestionCandidates();
  // This sorts by recency so that we keep the most recent entries and they
  // appear as suggestions in reverse chronological order.
  std::sort(suggestion_candidates.begin(), suggestion_candidates.end());

  std::vector<ContentSuggestion> suggestions;
  std::set<std::string> included_urls;
  std::map<std::string, int> suggestions_per_session;
  for (const SessionData& candidate : suggestion_candidates) {
    const std::string& session_tag = candidate.session->session_tag;
    auto duplicates_iter =
        included_urls.find(candidate.navigation->virtual_url().spec());
    auto count_iter = suggestions_per_session.find(session_tag);
    int count =
        count_iter == suggestions_per_session.end() ? 0 : count_iter->second;

    // Pick up to max (total and per device) tabs, and ensure no duplicates
    // are selected. This filtering must be done in a second pass because
    // this can cause newer tabs occluding less recent tabs, requiring more
    // than |max_foreign_tabs_per_device| to be considered per device.
    if (static_cast<int>(suggestions.size()) >= max_foreign_tabs_total ||
        duplicates_iter != included_urls.end() ||
        count >= max_foreign_tabs_per_device) {
      continue;
    }
    included_urls.insert(candidate.navigation->virtual_url().spec());
    suggestions_per_session[session_tag] = count + 1;
    suggestions.push_back(BuildSuggestion(candidate));
  }

  return suggestions;
}

std::vector<ForeignSessionsSuggestionsProvider::SessionData>
ForeignSessionsSuggestionsProvider::GetSuggestionCandidates() {
  // TODO(skym): If a tab was previously dismissed, but was since updated,
  // should it be resurrected and removed from the dismissed list? This would
  // likely require a change to the dismissed ids.
  // TODO(skym): No sense in keeping around dismissals for urls that no longer
  // exist on any current foreign devices. Should prune and save the pref back.
  const std::vector<const SyncedSession*>& foreign_sessions =
      foreign_sessions_provider_->GetAllForeignSessions();
  std::set<std::string> dismissed_ids = prefs::ReadDismissedIDsFromPrefs(
      *pref_service_, prefs::kDismissedForeignSessionsSuggestions);
  const TimeDelta max_foreign_tab_age = GetMaxForeignTabAge();
  std::vector<SessionData> suggestion_candidates;

  for (const SyncedSession* session : foreign_sessions) {
    for (const std::pair<const SessionID::id_type,
                         std::unique_ptr<sessions::SessionWindow>>& key_value :
         session->windows) {
      for (const std::unique_ptr<SessionTab>& tab : key_value.second->tabs) {
        if (tab->navigations.empty())
          continue;

        const SerializedNavigationEntry& navigation = tab->navigations.back();
        const std::string id = navigation.virtual_url().spec();
        // TODO(skym): Filter out internal pages. Tabs that contain only
        // non-syncable content should never reach the local client, but
        // sometimes the most recent navigation may be internal while one
        // of the previous ones was more valid.
        if (dismissed_ids.find(id) == dismissed_ids.end() &&
            (base::Time::Now() - tab->timestamp) < max_foreign_tab_age) {
          suggestion_candidates.push_back(
              SessionData{session, tab.get(), &navigation});
        }
      }
    }
  }
  return suggestion_candidates;
}

ContentSuggestion ForeignSessionsSuggestionsProvider::BuildSuggestion(
    const SessionData& data) {
  ContentSuggestion suggestion(provided_category_,
                               data.navigation->virtual_url().spec(),
                               data.navigation->virtual_url());
  suggestion.set_title(data.navigation->title());
  suggestion.set_publish_date(data.tab->timestamp);
  // TODO(skym): It's unclear if this simple approach is sufficient for
  // right-to-left languages.
  // This field is sandwiched between the url's favicon, which is on the left,
  // and the |publish_date|, which is to the right. The domain should always
  // appear next to the favicon.
  suggestion.set_publisher_name(
      base::UTF8ToUTF16(data.navigation->virtual_url().host() + " - " +
                        data.session->session_name));
  return suggestion;
}

}  // namespace ntp_snippets

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/bookmarks/bookmark_suggestions_provider.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

const int kMaxBookmarks = 10;
const int kMinBookmarks = 3;
const int kMaxBookmarkAgeInDays = 42;
const int kUseCreationDateFallbackForDays = 0;

const char* kMaxBookmarksParamName = "bookmarks_max_count";
const char* kMinBookmarksParamName = "bookmarks_min_count";
const char* kMaxBookmarkAgeInDaysParamName = "bookmarks_max_age_in_days";
const char* kUseCreationDateFallbackForDaysParamName =
    "bookmarks_creation_date_fallback_days";
const char* kShowIfEmptyParamName = "bookmarks_show_if_empty";

base::Time GetThresholdTime() {
  std::string age_in_days_string = variations::GetVariationParamValueByFeature(
    ntp_snippets::kBookmarkSuggestionsFeature, kMaxBookmarkAgeInDaysParamName);
  int age_in_days = 0;
  if (!base::StringToInt(age_in_days_string, &age_in_days)) {
    if (!age_in_days_string.empty())
      LOG(WARNING) << "Failed to parse bookmark age " << age_in_days_string;
    age_in_days = kMaxBookmarkAgeInDays;
  }
  return base::Time::Now() - base::TimeDelta::FromDays(age_in_days);
}

int UseCreationDateFallbackForDays() {
  std::string days_string = variations::GetVariationParamValueByFeature(
      ntp_snippets::kBookmarkSuggestionsFeature,
      kUseCreationDateFallbackForDaysParamName);
  int days = 0;
  if (!base::StringToInt(days_string, &days)) {
    if (!days_string.empty())
      LOG(WARNING) << "Failed to parse bookmark fallback days " << days_string;
    days = kUseCreationDateFallbackForDays;
  }
  return days;
}

int GetMaxCount() {
  std::string max_count_string = variations::GetVariationParamValueByFeature(
      ntp_snippets::kBookmarkSuggestionsFeature, kMaxBookmarksParamName);
  int max_count = 0;
  if (base::StringToInt(max_count_string, &max_count))
    return max_count;
  if (!max_count_string.empty())
    LOG(WARNING) << "Failed to parse max bookmarks count " << max_count_string;

  return kMaxBookmarks;
}

int GetMinCount() {
  std::string min_count_string = variations::GetVariationParamValueByFeature(
      ntp_snippets::kBookmarkSuggestionsFeature, kMinBookmarksParamName);
  int min_count = 0;
  if (base::StringToInt(min_count_string, &min_count))
    return min_count;
  if (!min_count_string.empty())
    LOG(WARNING) << "Failed to parse min bookmarks count " << min_count_string;

  return kMinBookmarks;
}

bool ShouldShowIfEmpty() {
  std::string show_if_empty = variations::GetVariationParamValueByFeature(
      ntp_snippets::kBookmarkSuggestionsFeature, kShowIfEmptyParamName);
  if (show_if_empty.empty() || show_if_empty == "false")
    return false;
  if (show_if_empty == "true")
    return true;

  LOG(WARNING) << "Failed to parse show if empty " << show_if_empty;
  return false;
}

}  // namespace

namespace ntp_snippets {

BookmarkSuggestionsProvider::BookmarkSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    CategoryFactory* category_factory,
    bookmarks::BookmarkModel* bookmark_model,
    PrefService* pref_service)
    : ContentSuggestionsProvider(observer, category_factory),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(
          category_factory->FromKnownCategory(KnownCategories::BOOKMARKS)),
      bookmark_model_(bookmark_model),
      fetch_requested_(false),
      end_of_list_last_visit_date_(GetThresholdTime()) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);
  base::Time first_m54_start;
  base::Time now = base::Time::Now();
  if (pref_service->HasPrefPath(prefs::kBookmarksFirstM54Start)) {
    first_m54_start = base::Time::FromInternalValue(
        pref_service->GetInt64(prefs::kBookmarksFirstM54Start));
  } else {
    first_m54_start = now;
    pref_service->SetInt64(prefs::kBookmarksFirstM54Start,
                           first_m54_start.ToInternalValue());
  }
  base::TimeDelta time_since_first_m54_start = now - first_m54_start;
  // Note: Setting the fallback timeout to zero effectively turns off the
  // fallback entirely.
  creation_date_fallback_ =
      time_since_first_m54_start.InDays() < UseCreationDateFallbackForDays();
  bookmark_model_->AddObserver(this);
  FetchBookmarks();
}

BookmarkSuggestionsProvider::~BookmarkSuggestionsProvider() {
  bookmark_model_->RemoveObserver(this);
}

// static
void BookmarkSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kBookmarksFirstM54Start, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

CategoryStatus BookmarkSuggestionsProvider::GetCategoryStatus(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  return category_status_;
}

CategoryInfo BookmarkSuggestionsProvider::GetCategoryInfo(Category category) {
  return CategoryInfo(
      l10n_util::GetStringUTF16(IDS_NTP_BOOKMARK_SUGGESTIONS_SECTION_HEADER),
      ContentSuggestionsCardLayout::MINIMAL_CARD,
      /* has_more_button */ true,
      /* show_if_empty */ ShouldShowIfEmpty() &&
          bookmark_model_->HasBookmarks());
  // TODO(treib): Setting show_if_empty to true is a temporary hack, see
  // crbug.com/640568.
}

void BookmarkSuggestionsProvider::DismissSuggestion(
    const std::string& suggestion_id) {
  DCHECK(bookmark_model_->loaded());
  GURL url(GetWithinCategoryIDFromUniqueID(suggestion_id));
  MarkBookmarksDismissed(bookmark_model_, url);
}

void BookmarkSuggestionsProvider::FetchSuggestionImage(
    const std::string& suggestion_id,
    const ImageFetchedCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, gfx::Image()));
}

void BookmarkSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  // TODO(vitaliii): Implement. See crbug.com/641321.
}

void BookmarkSuggestionsProvider::ClearCachedSuggestions(Category category) {
  DCHECK_EQ(category, provided_category_);
  // Ignored.
}

void BookmarkSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  DCHECK_EQ(category, provided_category_);
  std::vector<const BookmarkNode*> bookmarks =
      GetDismissedBookmarksForDebugging(bookmark_model_);

  std::vector<ContentSuggestion> suggestions;
  for (const BookmarkNode* bookmark : bookmarks)
    suggestions.emplace_back(ConvertBookmark(bookmark));
  callback.Run(std::move(suggestions));
}

void BookmarkSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  if (!bookmark_model_->loaded())
    return;
  MarkAllBookmarksUndismissed(bookmark_model_);
}

void BookmarkSuggestionsProvider::BookmarkModelLoaded(
    bookmarks::BookmarkModel* model,
    bool ids_reassigned) {
  DCHECK_EQ(bookmark_model_, model);
  if (fetch_requested_) {
    fetch_requested_ = false;
    FetchBookmarksInternal();
  }
}

void BookmarkSuggestionsProvider::OnWillChangeBookmarkMetaInfo(
    BookmarkModel* model,
    const BookmarkNode* node) {
  // Store the last visit date of the node that is about to change.
  node_to_change_last_visit_date_ =
      GetLastVisitDateForBookmarkIfNotDismissed(node, creation_date_fallback_);
}

void BookmarkSuggestionsProvider::BookmarkMetaInfoChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  base::Time time =
      GetLastVisitDateForBookmarkIfNotDismissed(node, creation_date_fallback_);
  if (time == node_to_change_last_visit_date_ ||
      time < end_of_list_last_visit_date_)
    return;

  // Last visit date of a node has changed (and is relevant for the list), we
  // should update the suggestions.
  FetchBookmarks();
}

void BookmarkSuggestionsProvider::BookmarkNodeRemoved(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      int old_index,
      const bookmarks::BookmarkNode* node,
      const std::set<GURL>& no_longer_bookmarked) {
  if (GetLastVisitDateForBookmarkIfNotDismissed(node, creation_date_fallback_) <
      end_of_list_last_visit_date_) {
    return;
  }

  // Some node from our list got deleted, we should update the suggestions.
  FetchBookmarks();
}

ContentSuggestion BookmarkSuggestionsProvider::ConvertBookmark(
    const BookmarkNode* bookmark) {
  ContentSuggestion suggestion(
      MakeUniqueID(provided_category_, bookmark->url().spec()),
      bookmark->url());

  suggestion.set_title(bookmark->GetTitle());
  suggestion.set_snippet_text(base::string16());
  suggestion.set_publish_date(
      GetLastVisitDateForBookmark(bookmark, creation_date_fallback_));
  suggestion.set_publisher_name(base::UTF8ToUTF16(bookmark->url().host()));
  return suggestion;
}

void BookmarkSuggestionsProvider::FetchBookmarksInternal() {
  DCHECK(bookmark_model_->loaded());

  NotifyStatusChanged(CategoryStatus::AVAILABLE);

  base::Time threshold_time = GetThresholdTime();
  std::vector<const BookmarkNode*> bookmarks =
      GetRecentlyVisitedBookmarks(bookmark_model_, GetMinCount(), GetMaxCount(),
                                  threshold_time, creation_date_fallback_);

  std::vector<ContentSuggestion> suggestions;
  for (const BookmarkNode* bookmark : bookmarks)
    suggestions.emplace_back(ConvertBookmark(bookmark));

  if (suggestions.empty())
    end_of_list_last_visit_date_ = threshold_time;
  else
    end_of_list_last_visit_date_ = suggestions.back().publish_date();

  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

void BookmarkSuggestionsProvider::FetchBookmarks() {
  if (bookmark_model_->loaded())
    FetchBookmarksInternal();
  else
    fetch_requested_ = true;
}

void BookmarkSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  if (category_status_ == new_status)
    return;
  category_status_ = new_status;
  observer()->OnCategoryStatusChanged(this, provided_category_, new_status);
}

}  // namespace ntp_snippets

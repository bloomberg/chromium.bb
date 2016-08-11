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
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/features.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

const int kMaxBookmarks = 10;
const int kMaxBookmarkAgeInDays = 42;

const char* kMaxBookmarksParamName = "max_count";
const char* kMaxBookmarkAgeInDaysParamName = "max_age_in_days";

base::Time GetThresholdTime() {
  std::string age_in_days_string = variations::GetVariationParamValueByFeature(
    ntp_snippets::kBookmarkSuggestionsFeature, kMaxBookmarkAgeInDaysParamName);
  int age_in_days = 0;
  if (!base::StringToInt(age_in_days_string, &age_in_days))
    age_in_days = kMaxBookmarkAgeInDays;

  return base::Time::Now() - base::TimeDelta::FromDays(age_in_days);
}

int GetMaxCount() {
  std::string max_count_string = variations::GetVariationParamValueByFeature(
    ntp_snippets::kBookmarkSuggestionsFeature, kMaxBookmarksParamName);
  int max_count = 0;
  if (base::StringToInt(max_count_string, &max_count))
    return max_count;

  return kMaxBookmarks;
}

}  // namespace

namespace ntp_snippets {

BookmarkSuggestionsProvider::BookmarkSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    CategoryFactory* category_factory,
    bookmarks::BookmarkModel* bookmark_model)
    : ContentSuggestionsProvider(observer, category_factory),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(
          category_factory->FromKnownCategory(KnownCategories::BOOKMARKS)),
      bookmark_model_(bookmark_model),
      fetch_requested_(false),
      end_of_list_last_visit_date_(GetThresholdTime()) {
  bookmark_model_->AddObserver(this);
  FetchBookmarks();
}

BookmarkSuggestionsProvider::~BookmarkSuggestionsProvider() {
  bookmark_model_->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

std::vector<Category> BookmarkSuggestionsProvider::GetProvidedCategories() {
  return std::vector<Category>({provided_category_});
}

CategoryStatus BookmarkSuggestionsProvider::GetCategoryStatus(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  return category_status_;
}

CategoryInfo BookmarkSuggestionsProvider::GetCategoryInfo(Category category) {
  return CategoryInfo(
      l10n_util::GetStringUTF16(IDS_NTP_BOOKMARK_SUGGESTIONS_SECTION_HEADER),
      ContentSuggestionsCardLayout::MINIMAL_CARD);
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
      FROM_HERE, base::Bind(callback, suggestion_id, gfx::Image()));
}

void BookmarkSuggestionsProvider::ClearCachedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  // Ignored.
}

std::vector<ContentSuggestion>
BookmarkSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  std::vector<const BookmarkNode*> bookmarks =
      GetDismissedBookmarksForDebugging(bookmark_model_);

  std::vector<ContentSuggestion> suggestions;
  for (const BookmarkNode* bookmark : bookmarks)
    suggestions.emplace_back(ConvertBookmark(bookmark));
  return suggestions;
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
      GetLastVisitDateForBookmarkIfNotDismissed(node);
}

void BookmarkSuggestionsProvider::BookmarkMetaInfoChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  base::Time time = GetLastVisitDateForBookmarkIfNotDismissed(node);
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
  if (GetLastVisitDateForBookmarkIfNotDismissed(node) <
      end_of_list_last_visit_date_)
    return;

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
  suggestion.set_publish_date(GetLastVisitDateForBookmark(bookmark));
  suggestion.set_publisher_name(base::UTF8ToUTF16(bookmark->url().host()));
  return suggestion;
}

void BookmarkSuggestionsProvider::FetchBookmarksInternal() {
  DCHECK(bookmark_model_->loaded());

  NotifyStatusChanged(CategoryStatus::AVAILABLE);

  base::Time threshold_time = GetThresholdTime();
  std::vector<const BookmarkNode*> bookmarks = GetRecentlyVisitedBookmarks(
      bookmark_model_, GetMaxCount(), threshold_time);

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

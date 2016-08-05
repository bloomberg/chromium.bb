// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/bookmarks/bookmark_suggestions_provider.h"

#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/content_suggestion.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

const int kMaxBookmarks = 10;
const int kMaxBookmarkAgeInDays = 42;

base::Time GetThresholdTime() {
  return base::Time::Now() - base::TimeDelta::FromDays(kMaxBookmarkAgeInDays);
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
  return category_status_;
}

void BookmarkSuggestionsProvider::DismissSuggestion(
    const std::string& suggestion_id) {
  // TODO(jkrcal): Implement blacklisting bookmarks until they are next visited.
  // Then also implement ClearDismissedSuggestionsForDebugging.
}

void BookmarkSuggestionsProvider::FetchSuggestionImage(
    const std::string& suggestion_id,
    const ImageFetchedCallback& callback) {
  callback.Run(suggestion_id, gfx::Image());
}

void BookmarkSuggestionsProvider::ClearCachedSuggestionsForDebugging() {
  // Ignored.
}

void BookmarkSuggestionsProvider::ClearDismissedSuggestionsForDebugging() {
  // TODO(jkrcal): Implement when discarded suggestions are supported.
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
  node_to_change_last_visit_date_ = GetLastVisitDateForBookmark(node);
}

void BookmarkSuggestionsProvider::BookmarkMetaInfoChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  base::Time time = GetLastVisitDateForBookmark(node);
  if (time == node_to_change_last_visit_date_ ||
      time < end_of_list_last_visit_date_)
    return;

  // Last visit date of a node has changed (and is relevant for the list), we
  // should update the suggestions.
  FetchBookmarks();
}

void BookmarkSuggestionsProvider::FetchBookmarksInternal() {
  DCHECK(bookmark_model_->loaded());

  NotifyStatusChanged(CategoryStatus::AVAILABLE);

  std::vector<const BookmarkNode*> bookmarks = GetRecentlyVisitedBookmarks(
      bookmark_model_, kMaxBookmarks, GetThresholdTime());

  std::vector<ContentSuggestion> suggestions;
  for (const BookmarkNode* bookmark : bookmarks) {
    ContentSuggestion suggestion(
        MakeUniqueID(provided_category_, bookmark->url().spec()),
        bookmark->url());

    suggestion.set_title(bookmark->GetTitle());
    suggestion.set_snippet_text(base::string16());
    suggestion.set_publish_date(GetLastVisitDateForBookmark(bookmark));
    suggestion.set_publisher_name(base::UTF8ToUTF16(bookmark->url().host()));
    suggestions.emplace_back(std::move(suggestion));
  }

  if (suggestions.empty())
    end_of_list_last_visit_date_ = GetThresholdTime();
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

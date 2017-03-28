// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/reading_list/reading_list_suggestions_provider.h"

#include <vector>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/ntp_snippets/category.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ntp_snippets {

ReadingListSuggestionsProvider::ReadingListSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    ReadingListModel* reading_list_model)
    : ContentSuggestionsProvider(observer),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(
          Category::FromKnownCategory(KnownCategories::READING_LIST)),
      reading_list_model_(reading_list_model) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);
  reading_list_model->AddObserver(this);
  if (reading_list_model_->loaded()) {
    FetchReadingListInternal();
  }
}

ReadingListSuggestionsProvider::~ReadingListSuggestionsProvider() {
  reading_list_model_->RemoveObserver(this);
}

CategoryStatus ReadingListSuggestionsProvider::GetCategoryStatus(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  return category_status_;
}

CategoryInfo ReadingListSuggestionsProvider::GetCategoryInfo(
    Category category) {
  DCHECK_EQ(category, provided_category_);

  return CategoryInfo(l10n_util::GetStringUTF16(
                          IDS_NTP_READING_LIST_SUGGESTIONS_SECTION_HEADER),
                      ContentSuggestionsCardLayout::FULL_CARD,
                      ContentSuggestionsAdditionalAction::VIEW_ALL,
                      /*show_if_empty=*/false,
                      l10n_util::GetStringUTF16(
                          IDS_NTP_READING_LIST_SUGGESTIONS_SECTION_EMPTY));
}

void ReadingListSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  // TODO(crbug.com/702241): Implement this method.
}

void ReadingListSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(crbug.com/702241): Implement this method.
}

void ReadingListSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  LOG(DFATAL) << "ReadingListSuggestionsProvider has no |Fetch| functionality!";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 Status(StatusCode::PERMANENT_ERROR,
                        "ReadingListSuggestionsProvider has no |Fetch| "
                        "functionality!"),
                 base::Passed(std::vector<ContentSuggestion>())));
}

void ReadingListSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  // TODO(crbug.com/702241): Implement this method.
}

void ReadingListSuggestionsProvider::ClearCachedSuggestions(Category category) {
  DCHECK_EQ(category, provided_category_);
  // Ignored.
}

void ReadingListSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  // TODO(crbug.com/702241): Implement this method.
}
void ReadingListSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  // TODO(crbug.com/702241): Implement this method.
}

void ReadingListSuggestionsProvider::ReadingListModelLoaded(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  FetchReadingListInternal();
}

void ReadingListSuggestionsProvider::FetchReadingListInternal() {
  // TODO(crbug.com/702241): Implement this method.
}

}  // namespace ntp_snippets

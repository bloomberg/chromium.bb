// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/reading_list/reading_list_suggestions_provider.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/reading_list/reading_list_distillation_state_util.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"

namespace ntp_snippets {

namespace {
// Max number of entries to return.
const int kMaxEntries = 3;

bool CompareEntries(const ReadingListEntry* lhs, const ReadingListEntry* rhs) {
  return lhs->UpdateTime() > rhs->UpdateTime();
}
}

ReadingListSuggestionsProvider::ReadingListSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    ReadingListModel* reading_list_model)
    : ContentSuggestionsProvider(observer),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(
          Category::FromKnownCategory(KnownCategories::READING_LIST)),
      reading_list_model_(reading_list_model),
      scoped_observer_(this) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);

  // If the ReadingListModel is loaded, this will trigger a call to
  // ReadingListModelLoaded. Keep it as last instruction.
  scoped_observer_.Add(reading_list_model_);
}

ReadingListSuggestionsProvider::~ReadingListSuggestionsProvider(){};

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

void ReadingListSuggestionsProvider::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  scoped_observer_.Remove(reading_list_model_);
  reading_list_model_ = nullptr;
}

void ReadingListSuggestionsProvider::FetchReadingListInternal() {
  if (!reading_list_model_)
    return;

  DCHECK(reading_list_model_->loaded());
  std::vector<const ReadingListEntry*> entries;
  for (const GURL& url : reading_list_model_->Keys()) {
    const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
    if (!entry->IsRead()) {
      entries.emplace_back(entry);
    }
  }

  if (entries.size() > kMaxEntries) {
    // Get the |kMaxEntries| most recent entries.
    std::partial_sort(entries.begin(), entries.begin() + kMaxEntries,
                      entries.end(), CompareEntries);
    entries.resize(kMaxEntries);
  } else {
    std::sort(entries.begin(), entries.end(), CompareEntries);
  }

  std::vector<ContentSuggestion> suggestions;
  for (const ReadingListEntry* entry : entries) {
    suggestions.emplace_back(ConvertEntry(entry));
  }

  NotifyStatusChanged(CategoryStatus::AVAILABLE);
  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

ContentSuggestion ReadingListSuggestionsProvider::ConvertEntry(
    const ReadingListEntry* entry) {
  ContentSuggestion suggestion(provided_category_, entry->URL().spec(),
                               entry->URL());

  if (!entry->Title().empty()) {
    suggestion.set_title(base::UTF8ToUTF16(entry->Title()));
  } else {
    suggestion.set_title(url_formatter::FormatUrl(entry->URL()));
  }
  suggestion.set_publisher_name(
      url_formatter::FormatUrl(entry->URL().GetOrigin()));

  auto extra = base::MakeUnique<ReadingListSuggestionExtra>();
  extra->distilled_state =
      SuggestionStateFromReadingListState(entry->DistilledState());
  extra->favicon_page_url =
      entry->DistilledURL().is_valid() ? entry->DistilledURL() : entry->URL();
  suggestion.set_reading_list_suggestion_extra(std::move(extra));

  return suggestion;
}

void ReadingListSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  if (category_status_ == new_status) {
    return;
  }
  category_status_ = new_status;
  observer()->OnCategoryStatusChanged(this, provided_category_, new_status);
}

}  // namespace ntp_snippets

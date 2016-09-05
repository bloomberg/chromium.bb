// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_service.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

ContentSuggestionsService::ContentSuggestionsService(
    State state,
    history::HistoryService* history_service)
    : state_(state), history_service_observer_(this) {
  // Can be null in tests.
  if (history_service)
    history_service_observer_.Add(history_service);
}

ContentSuggestionsService::~ContentSuggestionsService() {}

void ContentSuggestionsService::Shutdown() {
  ntp_snippets_service_ = nullptr;
  id_category_map_.clear();
  suggestions_by_category_.clear();
  providers_by_category_.clear();
  categories_.clear();
  providers_.clear();
  state_ = State::DISABLED;
  FOR_EACH_OBSERVER(Observer, observers_, ContentSuggestionsServiceShutdown());
}

CategoryStatus ContentSuggestionsService::GetCategoryStatus(
    Category category) const {
  if (state_ == State::DISABLED) {
    return CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED;
  }

  auto iterator = providers_by_category_.find(category);
  if (iterator == providers_by_category_.end())
    return CategoryStatus::NOT_PROVIDED;

  return iterator->second->GetCategoryStatus(category);
}

base::Optional<CategoryInfo> ContentSuggestionsService::GetCategoryInfo(
    Category category) const {
  auto iterator = providers_by_category_.find(category);
  if (iterator == providers_by_category_.end())
    return base::Optional<CategoryInfo>();
  return iterator->second->GetCategoryInfo(category);
}

const std::vector<ContentSuggestion>&
ContentSuggestionsService::GetSuggestionsForCategory(Category category) const {
  auto iterator = suggestions_by_category_.find(category);
  if (iterator == suggestions_by_category_.end())
    return no_suggestions_;
  return iterator->second;
}

void ContentSuggestionsService::FetchSuggestionImage(
    const std::string& suggestion_id,
    const ImageFetchedCallback& callback) {
  if (!id_category_map_.count(suggestion_id)) {
    LOG(WARNING) << "Requested image for unknown suggestion " << suggestion_id;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, gfx::Image()));
    return;
  }
  Category category = id_category_map_.at(suggestion_id);
  if (!providers_by_category_.count(category)) {
    LOG(WARNING) << "Requested image for suggestion " << suggestion_id
                 << " for unavailable category " << category;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, gfx::Image()));
    return;
  }
  providers_by_category_[category]->FetchSuggestionImage(suggestion_id,
                                                         callback);
}

void ContentSuggestionsService::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  for (const auto& provider : providers_) {
    provider->ClearHistory(begin, end, filter);
  }
}

void ContentSuggestionsService::ClearAllCachedSuggestions() {
  suggestions_by_category_.clear();
  id_category_map_.clear();
  for (const auto& category_provider_pair : providers_by_category_) {
    category_provider_pair.second->ClearCachedSuggestions(
        category_provider_pair.first);
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnNewSuggestions(category_provider_pair.first));
  }
}

void ContentSuggestionsService::ClearCachedSuggestions(
    Category category) {
  for (const ContentSuggestion& suggestion :
       suggestions_by_category_[category]) {
    id_category_map_.erase(suggestion.id());
  }
  suggestions_by_category_[category].clear();
  auto iterator = providers_by_category_.find(category);
  if (iterator != providers_by_category_.end())
    iterator->second->ClearCachedSuggestions(category);
}

void ContentSuggestionsService::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  auto iterator = providers_by_category_.find(category);
  if (iterator != providers_by_category_.end())
    iterator->second->GetDismissedSuggestionsForDebugging(category, callback);
  else
    callback.Run(std::vector<ContentSuggestion>());
}

void ContentSuggestionsService::ClearDismissedSuggestionsForDebugging(
    Category category) {
  auto iterator = providers_by_category_.find(category);
  if (iterator != providers_by_category_.end())
    iterator->second->ClearDismissedSuggestionsForDebugging(category);
}

void ContentSuggestionsService::DismissSuggestion(
    const std::string& suggestion_id) {
  if (!id_category_map_.count(suggestion_id)) {
    LOG(WARNING) << "Dismissed unknown suggestion " << suggestion_id;
    return;
  }
  Category category = id_category_map_.at(suggestion_id);
  if (!providers_by_category_.count(category)) {
    LOG(WARNING) << "Dismissed suggestion " << suggestion_id
                 << " for unavailable category " << category;
    return;
  }
  providers_by_category_[category]->DismissSuggestion(suggestion_id);

  // Remove the suggestion locally.
  bool removed = RemoveSuggestionByID(category, suggestion_id);
  DCHECK(removed) << "The dismissed suggestion " << suggestion_id
                  << " has already been removed. Providers must not call"
                  << " OnNewSuggestions in response to DismissSuggestion.";
}

void ContentSuggestionsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContentSuggestionsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContentSuggestionsService::RegisterProvider(
    std::unique_ptr<ContentSuggestionsProvider> provider) {
  DCHECK(state_ == State::ENABLED);
  providers_.push_back(std::move(provider));
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void ContentSuggestionsService::OnNewSuggestions(
    ContentSuggestionsProvider* provider,
    Category category,
    std::vector<ContentSuggestion> new_suggestions) {
  if (RegisterCategoryIfRequired(provider, category))
    NotifyCategoryStatusChanged(category);

  if (!IsCategoryStatusAvailable(provider->GetCategoryStatus(category)))
    return;

  for (const ContentSuggestion& suggestion :
       suggestions_by_category_[category]) {
    id_category_map_.erase(suggestion.id());
  }

  for (const ContentSuggestion& suggestion : new_suggestions)
    id_category_map_.insert(std::make_pair(suggestion.id(), category));

  suggestions_by_category_[category] = std::move(new_suggestions);

  // The positioning of the bookmarks category depends on whether it's empty.
  // TODO(treib): Remove this temporary hack, crbug.com/640568.
  if (category.IsKnownCategory(KnownCategories::BOOKMARKS))
    SortCategories();

  FOR_EACH_OBSERVER(Observer, observers_, OnNewSuggestions(category));
}

void ContentSuggestionsService::OnCategoryStatusChanged(
    ContentSuggestionsProvider* provider,
    Category category,
    CategoryStatus new_status) {
  if (!IsCategoryStatusAvailable(new_status)) {
    for (const ContentSuggestion& suggestion :
         suggestions_by_category_[category]) {
      id_category_map_.erase(suggestion.id());
    }
    suggestions_by_category_.erase(category);
  }
  if (new_status == CategoryStatus::NOT_PROVIDED) {
    auto providers_it = providers_by_category_.find(category);
    DCHECK(providers_it != providers_by_category_.end());
    DCHECK_EQ(provider, providers_it->second);
    providers_by_category_.erase(providers_it);
    categories_.erase(
        std::find(categories_.begin(), categories_.end(), category));
  } else {
    RegisterCategoryIfRequired(provider, category);
    DCHECK_EQ(new_status, provider->GetCategoryStatus(category));
  }
  NotifyCategoryStatusChanged(category);
}

void ContentSuggestionsService::OnSuggestionInvalidated(
    ContentSuggestionsProvider* provider,
    Category category,
    const std::string& suggestion_id) {
  RemoveSuggestionByID(category, suggestion_id);
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnSuggestionInvalidated(category, suggestion_id));
}

// history::HistoryServiceObserver implementation.
void ContentSuggestionsService::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  // We don't care about expired entries.
  if (expired)
    return;

  // Redirect to ClearHistory().
  if (all_history) {
    base::Time begin = base::Time();
    base::Time end = base::Time::Max();
    base::Callback<bool(const GURL& url)> filter =
        base::Bind([](const GURL& url) { return true; });
    ClearHistory(begin, end, filter);
  } else {
    if (deleted_rows.empty())
      return;

    base::Time begin = deleted_rows[0].last_visit();
    base::Time end = deleted_rows[0].last_visit();
    std::set<GURL> deleted_urls;
    for (const history::URLRow& row : deleted_rows) {
      if (row.last_visit() < begin)
        begin = row.last_visit();
      if (row.last_visit() > end)
        end = row.last_visit();
      deleted_urls.insert(row.url());
    }
    base::Callback<bool(const GURL& url)> filter = base::Bind(
        [](const std::set<GURL>& set, const GURL& url) {
          return set.count(url) != 0;
        },
        deleted_urls);
    ClearHistory(begin, end, filter);
  }
}

void ContentSuggestionsService::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observer_.RemoveAll();
}

bool ContentSuggestionsService::RegisterCategoryIfRequired(
    ContentSuggestionsProvider* provider,
    Category category) {
  auto it = providers_by_category_.find(category);
  if (it != providers_by_category_.end()) {
    DCHECK_EQ(it->second, provider);
    return false;
  }

  providers_by_category_[category] = provider;
  categories_.push_back(category);
  SortCategories();
  if (IsCategoryStatusAvailable(provider->GetCategoryStatus(category))) {
    suggestions_by_category_.insert(
        std::make_pair(category, std::vector<ContentSuggestion>()));
  }
  return true;
}

bool ContentSuggestionsService::RemoveSuggestionByID(
    Category category,
    const std::string& suggestion_id) {
  id_category_map_.erase(suggestion_id);
  std::vector<ContentSuggestion>* suggestions =
      &suggestions_by_category_[category];
  auto position =
      std::find_if(suggestions->begin(), suggestions->end(),
                   [&suggestion_id](const ContentSuggestion& suggestion) {
                     return suggestion_id == suggestion.id();
                   });
  if (position == suggestions->end())
    return false;
  suggestions->erase(position);

  // The positioning of the bookmarks category depends on whether it's empty.
  // TODO(treib): Remove this temporary hack, crbug.com/640568.
  if (category.IsKnownCategory(KnownCategories::BOOKMARKS))
    SortCategories();

  return true;
}

void ContentSuggestionsService::NotifyCategoryStatusChanged(Category category) {
  FOR_EACH_OBSERVER(
      Observer, observers_,
      OnCategoryStatusChanged(category, GetCategoryStatus(category)));
}

void ContentSuggestionsService::SortCategories() {
  auto it = suggestions_by_category_.find(
      category_factory_.FromKnownCategory(KnownCategories::BOOKMARKS));
  bool bookmarks_empty =
      (it == suggestions_by_category_.end() || it->second.empty());
  std::sort(
      categories_.begin(), categories_.end(),
      [this, bookmarks_empty](const Category& left, const Category& right) {
        // If the bookmarks section is empty, put it at the end.
        // TODO(treib): This is a temporary hack, see crbug.com/640568.
        if (bookmarks_empty) {
          if (left.IsKnownCategory(KnownCategories::BOOKMARKS))
            return false;
          if (right.IsKnownCategory(KnownCategories::BOOKMARKS))
            return true;
        }
        return category_factory_.CompareCategories(left, right);
      });
}

}  // namespace ntp_snippets

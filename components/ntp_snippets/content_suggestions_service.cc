// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_service.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

bool ContentSuggestionsService::CompareCategoriesByID::operator()(
    const Category& left,
    const Category& right) const {
  return left.id() < right.id();
}

ContentSuggestionsService::ContentSuggestionsService(State state)
    : state_(state) {}

ContentSuggestionsService::~ContentSuggestionsService() {}

void ContentSuggestionsService::Shutdown() {
  DCHECK(providers_.empty());
  DCHECK(categories_.empty());
  DCHECK(suggestions_by_category_.empty());
  DCHECK(id_category_map_.empty());
  state_ = State::DISABLED;
  FOR_EACH_OBSERVER(Observer, observers_, ContentSuggestionsServiceShutdown());
}

CategoryStatus ContentSuggestionsService::GetCategoryStatus(
    Category category) const {
  if (state_ == State::DISABLED) {
    return CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED;
  }

  auto iterator = providers_.find(category);
  if (iterator == providers_.end())
    return CategoryStatus::NOT_PROVIDED;

  return iterator->second->GetCategoryStatus(category);
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
    callback.Run(suggestion_id, gfx::Image());
    return;
  }
  Category category = id_category_map_.at(suggestion_id);
  if (!providers_.count(category)) {
    LOG(WARNING) << "Requested image for suggestion " << suggestion_id
                 << " for unavailable category " << category;
    callback.Run(suggestion_id, gfx::Image());
    return;
  }
  providers_[category]->FetchSuggestionImage(suggestion_id, callback);
}

void ContentSuggestionsService::ClearCachedSuggestionsForDebugging() {
  suggestions_by_category_.clear();
  id_category_map_.clear();
  for (auto& category_provider_pair : providers_) {
    category_provider_pair.second->ClearCachedSuggestionsForDebugging();
  }
  FOR_EACH_OBSERVER(Observer, observers_, OnNewSuggestions());
}

void ContentSuggestionsService::ClearDismissedSuggestionsForDebugging() {
  for (auto& category_provider_pair : providers_) {
    category_provider_pair.second->ClearDismissedSuggestionsForDebugging();
  }
}

void ContentSuggestionsService::DismissSuggestion(
    const std::string& suggestion_id) {
  if (!id_category_map_.count(suggestion_id)) {
    LOG(WARNING) << "Dismissed unknown suggestion " << suggestion_id;
    return;
  }
  Category category = id_category_map_.at(suggestion_id);
  if (!providers_.count(category)) {
    LOG(WARNING) << "Dismissed suggestion " << suggestion_id
                 << " for unavailable category " << category;
    return;
  }
  providers_[category]->DismissSuggestion(suggestion_id);

  // Remove the suggestion locally.
  id_category_map_.erase(suggestion_id);
  std::vector<ContentSuggestion>* suggestions =
      &suggestions_by_category_[category];
  auto position =
      std::find_if(suggestions->begin(), suggestions->end(),
                   [&suggestion_id](const ContentSuggestion& suggestion) {
                     return suggestion_id == suggestion.id();
                   });
  DCHECK(position != suggestions->end())
      << "The dismissed suggestion " << suggestion_id
      << " has already been removed. Providers must not call OnNewSuggestions"
         " in response to DismissSuggestion.";
  suggestions->erase(position);
}

void ContentSuggestionsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContentSuggestionsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContentSuggestionsService::RegisterProvider(
    ContentSuggestionsProvider* provider) {
  // TODO(pke): When NTPSnippetsService is purely a provider, think about
  // removing this state check.
  if (state_ == State::DISABLED)
    return;

  for (Category category : provider->GetProvidedCategories()) {
    DCHECK_EQ(0ul, providers_.count(category));
    providers_[category] = provider;
    categories_.push_back(category);
    if (IsCategoryStatusAvailable(provider->GetCategoryStatus(category))) {
      suggestions_by_category_[category] = std::vector<ContentSuggestion>();
    }
    NotifyCategoryStatusChanged(category);
  }
  std::sort(categories_.begin(), categories_.end(),
            [this](const Category& left, const Category& right) {
              return category_factory_.CompareCategories(left, right);
            });
  provider->SetObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void ContentSuggestionsService::OnNewSuggestions(
    Category changed_category,
    std::vector<ContentSuggestion> new_suggestions) {
  DCHECK(IsCategoryRegistered(changed_category));

  for (const ContentSuggestion& suggestion :
       suggestions_by_category_[changed_category]) {
    id_category_map_.erase(suggestion.id());
  }

  for (const ContentSuggestion& suggestion : new_suggestions) {
    id_category_map_.insert(std::make_pair(suggestion.id(), changed_category));
  }

  suggestions_by_category_[changed_category] = std::move(new_suggestions);

  FOR_EACH_OBSERVER(Observer, observers_, OnNewSuggestions());
}

void ContentSuggestionsService::OnCategoryStatusChanged(
    Category changed_category,
    CategoryStatus new_status) {
  if (!IsCategoryStatusAvailable(new_status)) {
    for (const ContentSuggestion& suggestion :
         suggestions_by_category_[changed_category]) {
      id_category_map_.erase(suggestion.id());
    }
    suggestions_by_category_.erase(changed_category);
  }
  NotifyCategoryStatusChanged(changed_category);
}

void ContentSuggestionsService::OnProviderShutdown(
    ContentSuggestionsProvider* provider) {
  for (Category category : provider->GetProvidedCategories()) {
    auto iterator = std::find(categories_.begin(), categories_.end(), category);
    DCHECK(iterator != categories_.end());
    categories_.erase(iterator);
    for (const ContentSuggestion& suggestion :
         suggestions_by_category_[category]) {
      id_category_map_.erase(suggestion.id());
    }
    suggestions_by_category_.erase(category);
    providers_.erase(category);
    NotifyCategoryStatusChanged(category);
  }
}

bool ContentSuggestionsService::IsCategoryRegistered(Category category) const {
  return std::find(categories_.begin(), categories_.end(), category) !=
         categories_.end();
}

void ContentSuggestionsService::NotifyCategoryStatusChanged(Category category) {
  FOR_EACH_OBSERVER(
      Observer, observers_,
      OnCategoryStatusChanged(category, GetCategoryStatus(category)));
}

}  // namespace ntp_snippets

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
  if (!providers_by_category_.count(category)) {
    LOG(WARNING) << "Requested image for suggestion " << suggestion_id
                 << " for unavailable category " << category;
    callback.Run(suggestion_id, gfx::Image());
    return;
  }
  providers_by_category_[category]->FetchSuggestionImage(suggestion_id,
                                                         callback);
}

void ContentSuggestionsService::ClearCachedSuggestionsForDebugging() {
  suggestions_by_category_.clear();
  id_category_map_.clear();
  for (auto& category_provider_pair : providers_by_category_) {
    category_provider_pair.second->ClearCachedSuggestionsForDebugging();
  }
  FOR_EACH_OBSERVER(Observer, observers_, OnNewSuggestions());
}

void ContentSuggestionsService::ClearDismissedSuggestionsForDebugging() {
  for (auto& category_provider_pair : providers_by_category_) {
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
  if (!providers_by_category_.count(category)) {
    LOG(WARNING) << "Dismissed suggestion " << suggestion_id
                 << " for unavailable category " << category;
    return;
  }
  providers_by_category_[category]->DismissSuggestion(suggestion_id);

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
    std::unique_ptr<ContentSuggestionsProvider> provider) {
  DCHECK(state_ == State::ENABLED);
  for (Category category : provider->GetProvidedCategories()) {
    DCHECK_NE(CategoryStatus::NOT_PROVIDED,
              provider->GetCategoryStatus(category));
    RegisterCategoryIfRequired(provider.get(), category);
    NotifyCategoryStatusChanged(category);
  }
  providers_.push_back(std::move(provider));
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void ContentSuggestionsService::OnNewSuggestions(
    ContentSuggestionsProvider* provider,
    Category category,
    std::vector<ContentSuggestion> new_suggestions) {
  if (RegisterCategoryIfRequired(provider, category)) {
    NotifyCategoryStatusChanged(category);
  }

  for (const ContentSuggestion& suggestion :
       suggestions_by_category_[category]) {
    id_category_map_.erase(suggestion.id());
  }

  for (const ContentSuggestion& suggestion : new_suggestions) {
    id_category_map_.insert(std::make_pair(suggestion.id(), category));
  }

  suggestions_by_category_[category] = std::move(new_suggestions);

  FOR_EACH_OBSERVER(Observer, observers_, OnNewSuggestions());
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
  std::sort(categories_.begin(), categories_.end(),
            [this](const Category& left, const Category& right) {
              return category_factory_.CompareCategories(left, right);
            });
  if (IsCategoryStatusAvailable(provider->GetCategoryStatus(category))) {
    suggestions_by_category_.insert(
        std::make_pair(category, std::vector<ContentSuggestion>()));
  }
  return true;
}

void ContentSuggestionsService::NotifyCategoryStatusChanged(Category category) {
  FOR_EACH_OBSERVER(
      Observer, observers_,
      OnCategoryStatusChanged(category, GetCategoryStatus(category)));
}

}  // namespace ntp_snippets

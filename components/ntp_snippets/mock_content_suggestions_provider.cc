// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/mock_content_suggestions_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"

namespace ntp_snippets {

MockContentSuggestionsProvider::MockContentSuggestionsProvider(
    Observer* observer,
    const std::vector<Category>& provided_categories)
    : ContentSuggestionsProvider(observer) {
  SetProvidedCategories(provided_categories);
}

MockContentSuggestionsProvider::~MockContentSuggestionsProvider() {}

void MockContentSuggestionsProvider::SetProvidedCategories(
    const std::vector<Category>& provided_categories) {
  statuses_.clear();
  provided_categories_ = provided_categories;
  for (Category category : provided_categories) {
    statuses_[category.id()] = CategoryStatus::AVAILABLE;
  }
}

CategoryStatus MockContentSuggestionsProvider::GetCategoryStatus(
    Category category) {
  return statuses_[category.id()];
}

CategoryInfo MockContentSuggestionsProvider::GetCategoryInfo(
    Category category) {
  return CategoryInfo(base::ASCIIToUTF16("Section title"),
                      ContentSuggestionsCardLayout::FULL_CARD,
                      /*has_fetch_action=*/true,
                      /*has_view_all_action=*/true,
                      /*show_if_empty=*/false,
                      base::ASCIIToUTF16("No suggestions message"));
}

void MockContentSuggestionsProvider::FireSuggestionsChanged(
    Category category,
    std::vector<ContentSuggestion> suggestions) {
  observer()->OnNewSuggestions(this, category, std::move(suggestions));
}

void MockContentSuggestionsProvider::FireCategoryStatusChanged(
    Category category,
    CategoryStatus new_status) {
  statuses_[category.id()] = new_status;
  observer()->OnCategoryStatusChanged(this, category, new_status);
}

void MockContentSuggestionsProvider::FireCategoryStatusChangedWithCurrentStatus(
    Category category) {
  observer()->OnCategoryStatusChanged(this, category, statuses_[category.id()]);
}

void MockContentSuggestionsProvider::FireSuggestionInvalidated(
    const ContentSuggestion::ID& suggestion_id) {
  observer()->OnSuggestionInvalidated(this, suggestion_id);
}

}  // namespace ntp_snippets

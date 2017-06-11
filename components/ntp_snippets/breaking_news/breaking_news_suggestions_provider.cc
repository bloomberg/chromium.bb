// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/breaking_news_suggestions_provider.h"

#include "base/bind.h"
#include "components/ntp_snippets/breaking_news/content_suggestions_gcm_app_handler.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/strings/grit/components_strings.h"

namespace ntp_snippets {

BreakingNewsSuggestionsProvider::BreakingNewsSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    std::unique_ptr<ContentSuggestionsGCMAppHandler> gcm_app_handler)
    : ContentSuggestionsProvider(observer),
      gcm_app_handler_(std::move(gcm_app_handler)),
      provided_category_(
          Category::FromKnownCategory(KnownCategories::BREAKING_NEWS)),
      category_status_(CategoryStatus::INITIALIZING) {}

BreakingNewsSuggestionsProvider::~BreakingNewsSuggestionsProvider() {
  gcm_app_handler_->StopListening();
}

void BreakingNewsSuggestionsProvider::Start() {
  // Unretained because |this| owns |gcm_app_handler_|.
  gcm_app_handler_->StartListening(
      base::Bind(&BreakingNewsSuggestionsProvider::OnNewContentSuggestion,
                 base::Unretained(this)));
}

void BreakingNewsSuggestionsProvider::OnNewContentSuggestion(
    const base::Value& content) {}

////////////////////////////////////////////////////////////////////////////////
// Private methods

CategoryStatus BreakingNewsSuggestionsProvider::GetCategoryStatus(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  return category_status_;
}

CategoryInfo BreakingNewsSuggestionsProvider::GetCategoryInfo(
    Category category) {
  // TODO(mamir): needs to be corrected, just a placeholer
  return CategoryInfo(base::string16(),
                      ContentSuggestionsCardLayout::MINIMAL_CARD,
                      ContentSuggestionsAdditionalAction::VIEW_ALL,
                      /*show_if_empty=*/false, base::string16());
}

void BreakingNewsSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {}

void BreakingNewsSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {}

void BreakingNewsSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {}

void BreakingNewsSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  // TODO(mamir): clear the provider.
}

void BreakingNewsSuggestionsProvider::ClearCachedSuggestions(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  // TODO(mamir): clear the cached suggestions.
}

void BreakingNewsSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {}

void BreakingNewsSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {}

}  // namespace ntp_snippets

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/breaking_news_suggestions_provider.h"

#include "base/bind.h"
#include "base/time/clock.h"
#include "components/ntp_snippets/breaking_news/breaking_news_listener.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/json_to_categories.h"
#include "components/strings/grit/components_strings.h"

namespace ntp_snippets {

BreakingNewsSuggestionsProvider::BreakingNewsSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    std::unique_ptr<BreakingNewsListener> breaking_news_raw_data_provider,
    std::unique_ptr<base::Clock> clock,
    std::unique_ptr<RemoteSuggestionsDatabase> database)
    : ContentSuggestionsProvider(observer),
      breaking_news_raw_data_provider_(
          std::move(breaking_news_raw_data_provider)),
      clock_(std::move(clock)),
      database_(std::move(database)),
      provided_category_(
          Category::FromKnownCategory(KnownCategories::BREAKING_NEWS)),
      category_status_(CategoryStatus::INITIALIZING) {
  database_->SetErrorCallback(
      base::Bind(&BreakingNewsSuggestionsProvider::OnDatabaseError,
                 base::Unretained(this)));
  database_->LoadSnippets(
      base::Bind(&BreakingNewsSuggestionsProvider::OnDatabaseLoaded,
                 base::Unretained(this)));
  // Unretained because |this| owns |breaking_news_listener_|.
  breaking_news_raw_data_provider_->StartListening(
      base::Bind(&BreakingNewsSuggestionsProvider::OnNewRemoteSuggestion,
                 base::Unretained(this)));
}

BreakingNewsSuggestionsProvider::~BreakingNewsSuggestionsProvider() {
  breaking_news_raw_data_provider_->StopListening();
}

void BreakingNewsSuggestionsProvider::OnNewRemoteSuggestion(
    std::unique_ptr<RemoteSuggestion> remote_suggestion) {
  std::vector<std::unique_ptr<RemoteSuggestion>> suggestions;
  suggestions.push_back(std::move(remote_suggestion));
  if (database_->IsInitialized()) {
    database_->SaveSnippets(suggestions);
  } else {
    // TODO(mamir): Check how often a breaking news is received before DB is
    // initialized.
    LOG(WARNING) << "Cannot store breaking news, database is not initialized.";
  }
  NotifyNewSuggestions(std::move(suggestions));
}

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
    const ContentSuggestion::ID& suggestion_id) {
  // TODO(mamir): implement.
}

void BreakingNewsSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(mamir): implement.
}

void BreakingNewsSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  // TODO(jkrcal): Make Fetch method optional.
}

void BreakingNewsSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  observer()->OnNewSuggestions(this, provided_category_,
                               std::vector<ContentSuggestion>());
}

void BreakingNewsSuggestionsProvider::ClearCachedSuggestions(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  // TODO(mamir): clear the cached suggestions.
}

void BreakingNewsSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  // TODO(mamir): implement.
}

void BreakingNewsSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  // TODO(mamir): implement.
}

void BreakingNewsSuggestionsProvider::OnDatabaseLoaded(
    std::vector<std::unique_ptr<RemoteSuggestion>> suggestions) {
  // TODO(mamir): check and update DB status.
  NotifyNewSuggestions(std::move(suggestions));
}

void BreakingNewsSuggestionsProvider::OnDatabaseError() {
  // TODO(mamir): implement.
}

void BreakingNewsSuggestionsProvider::NotifyNewSuggestions(
    std::vector<std::unique_ptr<RemoteSuggestion>> suggestions) {
  std::vector<ContentSuggestion> result;
  for (const std::unique_ptr<RemoteSuggestion>& suggestion : suggestions) {
    result.emplace_back(suggestion->ToContentSuggestion(provided_category_));
  }

  DVLOG(1) << "NotifyNewSuggestions(): " << result.size()
           << " items in category " << provided_category_;
  observer()->OnNewSuggestions(this, provided_category_, std::move(result));
}

}  // namespace ntp_snippets

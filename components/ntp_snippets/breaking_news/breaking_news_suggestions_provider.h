// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_BREAKING_NEWS_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_BREAKING_NEWS_SUGGESTIONS_PROVIDER_H_

#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/prefs/pref_registry_simple.h"

namespace ntp_snippets {
class BreakingNewsGCMAppHandler;
}

namespace base {
class Clock;
}

namespace ntp_snippets {

// Receives breaking news suggestions via GCM push messages, stores them and
// provides them as content suggestions.
class BreakingNewsSuggestionsProvider : public ContentSuggestionsProvider {
 public:
  BreakingNewsSuggestionsProvider(
      ContentSuggestionsProvider::Observer* observer,
      std::unique_ptr<BreakingNewsGCMAppHandler> gcm_app_handler,
      std::unique_ptr<base::Clock> clock,
      std::unique_ptr<RemoteSuggestionsDatabase> database);
  ~BreakingNewsSuggestionsProvider() override;

  // Starts the underlying GCM handler and registers the callback when GCM
  // receives a message.
  void Start();

 private:
  // ContentSuggestionsProvider implementation.
  CategoryStatus GetCategoryStatus(Category category) override;
  CategoryInfo GetCategoryInfo(Category category) override;
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id) override;
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            const ImageFetchedCallback& callback) override;
  void Fetch(const Category& category,
             const std::set<std::string>& known_suggestion_ids,
             const FetchDoneCallback& callback) override;
  void ClearHistory(
      base::Time begin,
      base::Time end,
      const base::Callback<bool(const GURL& url)>& filter) override;
  void ClearCachedSuggestions(Category category) override;
  void GetDismissedSuggestionsForDebugging(
      Category category,
      const DismissedSuggestionsCallback& callback) override;
  void ClearDismissedSuggestionsForDebugging(Category category) override;

  // Callback called from the GCM handler when new content has been pushed from
  // the server.
  void OnNewContentSuggestion(std::unique_ptr<base::Value> content);

  // Callbacks for the RemoteSuggestionsDatabase.
  void OnDatabaseLoaded(
      std::vector<std::unique_ptr<RemoteSuggestion>> suggestions);
  void OnDatabaseError();

  void NotifyNewSuggestions(
      std::vector<std::unique_ptr<RemoteSuggestion>> suggestions);

  std::unique_ptr<BreakingNewsGCMAppHandler> gcm_app_handler_;
  std::unique_ptr<base::Clock> clock_;

  // The database for persisting suggestions.
  std::unique_ptr<RemoteSuggestionsDatabase> database_;

  const Category provided_category_;
  CategoryStatus category_status_;

  DISALLOW_COPY_AND_ASSIGN(BreakingNewsSuggestionsProvider);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_BREAKING_NEWS_SUGGESTIONS_PROVIDER_H_

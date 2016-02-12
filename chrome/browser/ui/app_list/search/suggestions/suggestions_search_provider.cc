// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/suggestions/suggestions_search_provider.h"

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search/suggestions/suggestions_utils.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_list/search/suggestions/url_suggestion_result.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_service.h"
#include "components/suggestions/suggestions_utils.h"

namespace app_list {

SuggestionsSearchProvider::SuggestionsSearchProvider(
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : profile_(profile),
      list_controller_(list_controller),
      favicon_service_(FaviconServiceFactory::GetForProfile(
          profile,
          ServiceAccessType::EXPLICIT_ACCESS)),
      suggestions_service_(
          suggestions::SuggestionsServiceFactory::GetForProfile(profile)),
      weak_ptr_factory_(this) {
}

SuggestionsSearchProvider::~SuggestionsSearchProvider() {
}

void SuggestionsSearchProvider::Start(bool /*is_voice_query*/,
                                      const base::string16& query) {
  ClearResults();
  // If the service is not enabled, do not return any results.
  if (!suggestions_service_)
    return;

  // Only return suggestions on an empty query.
  if (!query.empty())
    return;

  // Suggestions service is enabled; initiate a query.
  suggestions_service_->FetchSuggestionsData(
      suggestions::GetSyncState(profile_),
      base::Bind(&SuggestionsSearchProvider::OnSuggestionsProfileAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SuggestionsSearchProvider::Stop() {
}

void SuggestionsSearchProvider::OnSuggestionsProfileAvailable(
    const suggestions::SuggestionsProfile& suggestions_profile) {
  for (int i = 0; i < suggestions_profile.suggestions_size(); ++i) {
    const suggestions::ChromeSuggestion& suggestion =
        suggestions_profile.suggestions(i);

    // TODO(mathp): If it's an app, create an AppResult.
    scoped_ptr<URLSuggestionResult> result(new URLSuggestionResult(
        profile_, list_controller_, favicon_service_, suggestions_service_,
        suggestion));
    result->set_relevance(1.0 / (i + 1));
    Add(std::move(result));
  }
}

}  // namespace app_list

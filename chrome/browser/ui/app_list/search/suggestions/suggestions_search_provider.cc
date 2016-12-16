// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/suggestions/suggestions_search_provider.h"

#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_list/search/suggestions/url_suggestion_result.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_service.h"

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

  // Only return suggestions on an empty query.
  if (!query.empty())
    return;

  const suggestions::SuggestionsProfile suggestions_profile =
      suggestions_service_->GetSuggestionsDataFromCache().value_or(
          suggestions::SuggestionsProfile());
  for (int i = 0; i < suggestions_profile.suggestions_size(); ++i) {
    const suggestions::ChromeSuggestion& suggestion =
        suggestions_profile.suggestions(i);

    // TODO(mathp): If it's an app, create an AppResult.
    std::unique_ptr<URLSuggestionResult> result(
        new URLSuggestionResult(profile_, list_controller_, favicon_service_,
                                suggestions_service_, suggestion));
    result->set_relevance(1.0 / (i + 1));
    Add(std::move(result));
  }
}

void SuggestionsSearchProvider::Stop() {
}

}  // namespace app_list

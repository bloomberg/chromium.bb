// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"

#include <unordered_set>

#include "base/memory/ptr_util.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"

using ntp_snippets::Category;
using ntp_snippets::ContentSuggestion;

namespace offline_pages {

namespace {

const ntp_snippets::Category& ArticlesCategory() {
  static ntp_snippets::Category articles =
      Category::FromKnownCategory(ntp_snippets::KnownCategories::ARTICLES);
  return articles;
}

ClientId CreateClientIDFromSuggestionId(const ContentSuggestion::ID& id) {
  return ClientId(kSuggestedArticlesNamespace, id.id_within_category());
}

}  // namespace

SuggestedArticlesObserver::SuggestedArticlesObserver(
    ntp_snippets::ContentSuggestionsService* content_suggestions_service,
    PrefetchService* prefetch_service)
    : content_suggestions_service_(content_suggestions_service),
      prefetch_service_(prefetch_service) {
  // The content suggestions service can be |nullptr| in tests.
  if (content_suggestions_service_)
    content_suggestions_service_->AddObserver(this);
}

SuggestedArticlesObserver::~SuggestedArticlesObserver() = default;

void SuggestedArticlesObserver::OnNewSuggestions(Category category) {
  // TODO(dewittj): Change this to check whether a given category is not
  // a _remote_ category.
  if (category != ArticlesCategory() ||
      category_status_ != ntp_snippets::CategoryStatus::AVAILABLE) {
    return;
  }

  const std::vector<ContentSuggestion>& suggestions =
      test_articles_ ? *test_articles_
                     : content_suggestions_service_->GetSuggestionsForCategory(
                           ArticlesCategory());
  if (suggestions.empty())
    return;

  std::vector<PrefetchDispatcher::PrefetchURL> prefetch_urls;
  for (const ContentSuggestion& suggestion : suggestions) {
    prefetch_urls.push_back(
        {CreateClientIDFromSuggestionId(suggestion.id()), suggestion.url()});
  }

  prefetch_service_->GetDispatcher()->AddCandidatePrefetchURLs(prefetch_urls);
}

void SuggestedArticlesObserver::OnCategoryStatusChanged(
    Category category,
    ntp_snippets::CategoryStatus new_status) {
  if (category != ArticlesCategory() || category_status_ == new_status)
    return;

  category_status_ = new_status;

  if (category_status_ ==
          ntp_snippets::CategoryStatus::CATEGORY_EXPLICITLY_DISABLED ||
      category_status_ ==
          ntp_snippets::CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED) {
    prefetch_service_->GetDispatcher()->RemoveAllUnprocessedPrefetchURLs(
        kSuggestedArticlesNamespace);
  }
}

void SuggestedArticlesObserver::OnSuggestionInvalidated(
    const ContentSuggestion::ID& suggestion_id) {
  prefetch_service_->GetDispatcher()->RemovePrefetchURLsByClientId(
      CreateClientIDFromSuggestionId(suggestion_id));
}

void SuggestedArticlesObserver::OnFullRefreshRequired() {
  prefetch_service_->GetDispatcher()->RemoveAllUnprocessedPrefetchURLs(
      kSuggestedArticlesNamespace);
  OnNewSuggestions(ArticlesCategory());
}

void SuggestedArticlesObserver::ContentSuggestionsServiceShutdown() {
  // No need to do anything here, we will just stop getting events.
}

std::vector<ntp_snippets::ContentSuggestion>*
SuggestedArticlesObserver::GetTestingArticles() {
  if (!test_articles_) {
    test_articles_ =
        base::MakeUnique<std::vector<ntp_snippets::ContentSuggestion>>();
  }
  return test_articles_.get();
}

}  // namespace offline_pages

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/offline_page_suggestions_provider.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"

using offline_pages::MultipleOfflinePageItemResult;
using offline_pages::OfflinePageModel;
using offline_pages::OfflinePageItem;

namespace ntp_snippets {

OfflinePageSuggestionsProvider::OfflinePageSuggestionsProvider(
    OfflinePageModel* offline_page_model)
    : ContentSuggestionsProvider({ContentSuggestionsCategory::OFFLINE_PAGES}),
      category_status_(ContentSuggestionsCategoryStatus::AVAILABLE_LOADING),
      observer_(nullptr),
      offline_page_model_(offline_page_model) {
  offline_page_model_->AddObserver(this);
}

OfflinePageSuggestionsProvider::~OfflinePageSuggestionsProvider() {}

// Inherited from KeyedService.
void OfflinePageSuggestionsProvider::Shutdown() {
  offline_page_model_->RemoveObserver(this);
  category_status_ = ContentSuggestionsCategoryStatus::NOT_PROVIDED;
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void OfflinePageSuggestionsProvider::SetObserver(
    ContentSuggestionsProvider::Observer* observer) {
  observer_ = observer;
  if (observer)
    FetchOfflinePages();
}

ContentSuggestionsCategoryStatus
OfflinePageSuggestionsProvider::GetCategoryStatus(
    ContentSuggestionsCategory category) {
  return category_status_;
}

void OfflinePageSuggestionsProvider::DiscardSuggestion(
    const std::string& suggestion_id) {
  // TODO(pke): Implement some "dont show on NTP anymore" behaviour,
  // then also implement ClearDiscardedSuggestionsForDebugging.
}

void OfflinePageSuggestionsProvider::FetchSuggestionImage(
    const std::string& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(pke): Implement.
}

void OfflinePageSuggestionsProvider::ClearCachedSuggestionsForDebugging() {
  // Ignored.
}

void OfflinePageSuggestionsProvider::ClearDiscardedSuggestionsForDebugging() {
  // TODO(pke): Implement when discarded suggestions are supported.
}

void OfflinePageSuggestionsProvider::OfflinePageModelLoaded(
    OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
}

void OfflinePageSuggestionsProvider::OfflinePageModelChanged(
    OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  FetchOfflinePages();
}

void OfflinePageSuggestionsProvider::OfflinePageDeleted(
    int64_t offline_id,
    const offline_pages::ClientId& client_id) {
  // TODO(pke): Implement, suggestion has to be removed from UI immediately.
}

void OfflinePageSuggestionsProvider::FetchOfflinePages() {
  offline_page_model_->GetAllPages(
      base::Bind(&OfflinePageSuggestionsProvider::OnOfflinePagesLoaded,
                 base::Unretained(this)));
}

void OfflinePageSuggestionsProvider::OnOfflinePagesLoaded(
    const MultipleOfflinePageItemResult& result) {
  NotifyStatusChanged(ContentSuggestionsCategoryStatus::AVAILABLE);
  if (!observer_)
    return;

  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem& item : result) {
    ContentSuggestion suggestion(
        MakeUniqueID(ContentSuggestionsCategory::OFFLINE_PAGES,
                     base::IntToString(item.offline_id)),
        item.GetOfflineURL());

    // TODO(pke): Sort my most recently visited and only keep the top one of
    // multiple entries for the same URL.
    // TODO(pke): Get more reasonable data from the OfflinePageModel here.
    suggestion.set_title(item.url.spec());
    suggestion.set_snippet_text(std::string());
    suggestion.set_publish_date(item.creation_time);
    suggestion.set_publisher_name(item.url.host());
    suggestions.emplace_back(std::move(suggestion));
  }

  observer_->OnNewSuggestions(ContentSuggestionsCategory::OFFLINE_PAGES,
                              std::move(suggestions));
}

void OfflinePageSuggestionsProvider::NotifyStatusChanged(
    ContentSuggestionsCategoryStatus new_status) {
  if (category_status_ == new_status)
    return;
  category_status_ = new_status;

  if (!observer_)
    return;
  observer_->OnCategoryStatusChanged(ContentSuggestionsCategory::OFFLINE_PAGES,
                                     new_status);
}

}  // namespace ntp_snippets

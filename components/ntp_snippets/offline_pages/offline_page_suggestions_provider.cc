// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/offline_page_suggestions_provider.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"

using offline_pages::MultipleOfflinePageItemResult;
using offline_pages::OfflinePageModel;
using offline_pages::OfflinePageItem;

namespace ntp_snippets {

namespace {

const int kMaxSuggestionsCount = 5;

}  // namespace

OfflinePageSuggestionsProvider::OfflinePageSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    CategoryFactory* category_factory,
    OfflinePageModel* offline_page_model)
    : ContentSuggestionsProvider(observer, category_factory),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      offline_page_model_(offline_page_model),
      provided_category_(
          category_factory->FromKnownCategory(KnownCategories::OFFLINE_PAGES)) {
  offline_page_model_->AddObserver(this);
  FetchOfflinePages();
}

OfflinePageSuggestionsProvider::~OfflinePageSuggestionsProvider() {
  offline_page_model_->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

std::vector<Category> OfflinePageSuggestionsProvider::GetProvidedCategories() {
  return std::vector<Category>({provided_category_});
}

CategoryStatus OfflinePageSuggestionsProvider::GetCategoryStatus(
    Category category) {
  return category_status_;
}

void OfflinePageSuggestionsProvider::DismissSuggestion(
    const std::string& suggestion_id) {
  // TODO(pke): Implement some "dont show on NTP anymore" behaviour,
  // then also implement ClearDismissedSuggestionsForDebugging.
}

void OfflinePageSuggestionsProvider::FetchSuggestionImage(
    const std::string& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(pke): Implement.
}

void OfflinePageSuggestionsProvider::ClearCachedSuggestionsForDebugging() {
  // Ignored.
}

void OfflinePageSuggestionsProvider::ClearDismissedSuggestionsForDebugging() {
  // TODO(pke): Implement when dismissed suggestions are supported.
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
  NotifyStatusChanged(CategoryStatus::AVAILABLE);

  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem& item : result) {
    // TODO(pke): Make sure the URL is actually opened as an offline URL.
    // Currently, the browser opens the offline URL and then immediately
    // redirects to the online URL if the device is online.
    ContentSuggestion suggestion(
        MakeUniqueID(provided_category_, base::IntToString(item.offline_id)),
        item.GetOfflineURL());

    // TODO(pke): Sort my most recently visited and only keep the top one of
    // multiple entries for the same URL.
    // TODO(pke): Get more reasonable data from the OfflinePageModel here.
    suggestion.set_title(base::UTF8ToUTF16(item.url.spec()));
    suggestion.set_snippet_text(base::string16());
    suggestion.set_publish_date(item.creation_time);
    suggestion.set_publisher_name(base::UTF8ToUTF16(item.url.host()));
    suggestions.emplace_back(std::move(suggestion));
    if (suggestions.size() == kMaxSuggestionsCount)
      break;
  }

  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

void OfflinePageSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  if (category_status_ == new_status)
    return;
  category_status_ = new_status;

  observer()->OnCategoryStatusChanged(this, provided_category_, new_status);
}

}  // namespace ntp_snippets

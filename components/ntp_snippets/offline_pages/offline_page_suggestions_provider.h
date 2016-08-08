// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_OFFLINE_PAGE_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_OFFLINE_PAGE_SUGGESTIONS_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_types.h"

namespace gfx {
class Image;
}

namespace ntp_snippets {

// Provides content suggestions from the offline pages model.
// Currently, those are only the pages that the user last navigated to in an
// open tab and offlined bookmarks.
class OfflinePageSuggestionsProvider
    : public ContentSuggestionsProvider,
      public offline_pages::OfflinePageModel::Observer {
 public:
  OfflinePageSuggestionsProvider(
      ContentSuggestionsProvider::Observer* observer,
      CategoryFactory* category_factory,
      offline_pages::OfflinePageModel* offline_page_model);
  ~OfflinePageSuggestionsProvider() override;

 private:
  // ContentSuggestionsProvider implementation.
  std::vector<Category> GetProvidedCategories() override;
  CategoryStatus GetCategoryStatus(Category category) override;
  void DismissSuggestion(const std::string& suggestion_id) override;
  void FetchSuggestionImage(const std::string& suggestion_id,
                            const ImageFetchedCallback& callback) override;
  void ClearCachedSuggestionsForDebugging() override;
  void ClearDismissedSuggestionsForDebugging() override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(offline_pages::OfflinePageModel* model) override;
  void OfflinePageModelChanged(offline_pages::OfflinePageModel* model) override;
  void OfflinePageDeleted(int64_t offline_id,
                          const offline_pages::ClientId& client_id) override;

  // Queries the OfflinePageModel for offline pages.
  void FetchOfflinePages();

  // Callback from the OfflinePageModel.
  void OnOfflinePagesLoaded(
      const offline_pages::MultipleOfflinePageItemResult& result);

  // Updates the |category_status_| and notifies the |observer_|, if necessary.
  void NotifyStatusChanged(CategoryStatus new_status);

  CategoryStatus category_status_;

  offline_pages::OfflinePageModel* offline_page_model_;

  const Category provided_category_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageSuggestionsProvider);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_OFFLINE_PAGE_SUGGESTIONS_PROVIDER_H_

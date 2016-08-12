// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_OFFLINE_PAGE_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_OFFLINE_PAGE_SUGGESTIONS_PROVIDER_H_

#include <set>
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

class PrefRegistrySimple;
class PrefService;

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
      bool recent_tabs_enabled,
      bool downloads_enabled,
      bool download_manager_ui_enabled,
      ContentSuggestionsProvider::Observer* observer,
      CategoryFactory* category_factory,
      offline_pages::OfflinePageModel* offline_page_model,
      PrefService* pref_service);
  ~OfflinePageSuggestionsProvider() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // ContentSuggestionsProvider implementation.
  std::vector<Category> GetProvidedCategories() override;
  CategoryStatus GetCategoryStatus(Category category) override;
  CategoryInfo GetCategoryInfo(Category category) override;
  void DismissSuggestion(const std::string& suggestion_id) override;
  void FetchSuggestionImage(const std::string& suggestion_id,
                            const ImageFetchedCallback& callback) override;
  void ClearCachedSuggestionsForDebugging(Category category) override;
  std::vector<ContentSuggestion> GetDismissedSuggestionsForDebugging(
      Category category) override;
  void ClearDismissedSuggestionsForDebugging(Category category) override;

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

  // Updates the |category_status_| of the |category| and notifies the
  // |observer_|, if necessary.
  void NotifyStatusChanged(Category category, CategoryStatus new_status);

  // Converts an OfflinePageItem to a ContentSuggestion for the |category|.
  ContentSuggestion ConvertOfflinePage(
      Category category,
      const offline_pages::OfflinePageItem& offline_page) const;

  // Gets the |kMaxSuggestionsCount| most recently visited OfflinePageItems from
  // the list, orders them by last visit date and converts them to
  // ContentSuggestions for the |category|.
  std::vector<ContentSuggestion> GetMostRecentlyVisited(
      Category category,
      std::vector<const offline_pages::OfflinePageItem*> offline_page_items)
      const;

  // Reads dismissed IDs from the pref with name |pref_name|.
  std::set<std::string> ReadDismissedIDsFromPrefs(
      const std::string& pref_name) const;

  // Writes |dismissed_ids| to the pref with name |pref_name|.
  void StoreDismissedIDsToPrefs(const std::string& pref_name,
                                const std::set<std::string>& dismissed_ids);

  CategoryStatus recent_tabs_status_;
  CategoryStatus downloads_status_;

  offline_pages::OfflinePageModel* offline_page_model_;

  const Category recent_tabs_category_;
  const Category downloads_category_;

  PrefService* pref_service_;

  std::set<std::string> dismissed_recent_tab_ids_;
  std::set<std::string> dismissed_download_ids_;

  // Whether the Download Manager UI is enabled, in which case the More button
  // for the Downloads section can redirect there.
  const bool download_manager_ui_enabled_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageSuggestionsProvider);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_OFFLINE_PAGE_SUGGESTIONS_PROVIDER_H_

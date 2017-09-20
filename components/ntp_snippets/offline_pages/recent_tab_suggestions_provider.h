// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_RECENT_TAB_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_RECENT_TAB_SUGGESTIONS_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/offline_pages/core/downloads/download_ui_adapter.h"
#include "components/offline_pages/core/offline_page_model.h"

class PrefRegistrySimple;
class PrefService;

using ContentId = offline_items_collection::ContentId;
using OfflineItem = offline_items_collection::OfflineItem;
using OfflineContentProvider = offline_items_collection::OfflineContentProvider;

namespace ntp_snippets {

// Provides recent tabs content suggestions from the offline pages model.
class RecentTabSuggestionsProvider : public ContentSuggestionsProvider,
                                     public OfflineContentProvider::Observer {
 public:
  RecentTabSuggestionsProvider(ContentSuggestionsProvider::Observer* observer,
                               offline_pages::DownloadUIAdapter* ui_adapter,
                               PrefService* pref_service);
  ~RecentTabSuggestionsProvider() override;

  // ContentSuggestionsProvider implementation.
  CategoryStatus GetCategoryStatus(Category category) override;
  CategoryInfo GetCategoryInfo(Category category) override;
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id) override;
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            ImageFetchedCallback callback) override;
  void Fetch(const Category& category,
             const std::set<std::string>& known_suggestion_ids,
             FetchDoneCallback callback) override;
  void ClearHistory(
      base::Time begin,
      base::Time end,
      const base::Callback<bool(const GURL& url)>& filter) override;
  void ClearCachedSuggestions(Category category) override;
  void GetDismissedSuggestionsForDebugging(
      Category category,
      DismissedSuggestionsCallback callback) override;
  void ClearDismissedSuggestionsForDebugging(Category category) override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  friend class RecentTabSuggestionsProviderTestNoLoad;

  // OfflineContentProvider::Observer implementation.
  void OnItemsAvailable(OfflineContentProvider* provider) override;
  void OnItemsAdded(const std::vector<OfflineItem>& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item) override;

  // Updates the |category_status_| of the |provided_category_| and notifies the
  // |observer_|, if necessary.
  void NotifyStatusChanged(CategoryStatus new_status);

  // Manually requests all Recent Tabs UI items and updates the suggestions.
  void FetchRecentTabs();

  // Converts an OfflineItem to a ContentSuggestion for the
  // |provided_category_|.
  ContentSuggestion ConvertUIItem(const OfflineItem& ui_item) const;

  // Removes duplicates for the same URL leaving only the most recently created
  // items, returns at most |GetMaxSuggestionsCount()| ContentSuggestions
  // corresponding to the remaining items, sorted by creation time (newer
  // first).
  std::vector<ContentSuggestion> GetMostRecentlyCreatedWithoutDuplicates(
      std::vector<OfflineItem>& ui_items) const;

  // Fires the |OnSuggestionInvalidated| event for the suggestion corresponding
  // to the given |offline_id| and deletes it from the dismissed IDs list, if
  // necessary.
  void InvalidateSuggestion(const std::string& ui_item_guid);

  // Reads dismissed IDs from Prefs.
  std::set<std::string> ReadDismissedIDsFromPrefs() const;

  // Writes |dismissed_ids| into Prefs.
  void StoreDismissedIDsToPrefs(const std::set<std::string>& dismissed_ids);

  CategoryStatus category_status_;
  const Category provided_category_;
  offline_pages::DownloadUIAdapter* recent_tabs_ui_adapter_;

  PrefService* pref_service_;

  base::WeakPtrFactory<RecentTabSuggestionsProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabSuggestionsProvider);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_OFFLINE_PAGES_RECENT_TAB_SUGGESTIONS_PROVIDER_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_PHYSICAL_WEB_PAGES_PHYSICAL_WEB_PAGE_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_PHYSICAL_WEB_PAGES_PHYSICAL_WEB_PAGE_SUGGESTIONS_PROVIDER_H_

#include <map>
#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/physical_web/data_source/physical_web_listener.h"

class GURL;
class PrefRegistrySimple;
class PrefService;

namespace physical_web {
class PhysicalWebDataSource;
struct Metadata;
}  // namespace physical_web

namespace ntp_snippets {

// Provides content suggestions from the Physical Web Service.
class PhysicalWebPageSuggestionsProvider
    : public ContentSuggestionsProvider,
      public physical_web::PhysicalWebListener {
 public:
  PhysicalWebPageSuggestionsProvider(
      ContentSuggestionsProvider::Observer* observer,
      physical_web::PhysicalWebDataSource* physical_web_data_source,
      PrefService* pref_service);
  ~PhysicalWebPageSuggestionsProvider() override;

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

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  friend class PhysicalWebPageSuggestionsProviderTest;

  // Updates the |category_status_| and notifies the |observer_|, if necessary.
  void NotifyStatusChanged(CategoryStatus new_status);

  // Manually requests all physical web pages and updates the suggestions.
  void FetchPhysicalWebPages();

  // Returns at most |max_count| ContentSuggestions with IDs not in
  // |excluded_ids| and sorted by distance (the closest first). Dismissed
  // suggestions are excluded automatically (no need to add them to
  // |excluded_ids|) and pruned. The raw pages are obtained from Physical Web
  // data source.
  std::vector<ContentSuggestion> GetMostRecentPhysicalWebPagesWithFilter(
      int max_count,
      const std::set<std::string>& excluded_ids);

  // Converts an Physical Web page to a ContentSuggestion.
  ContentSuggestion ConvertPhysicalWebPage(
      const physical_web::Metadata& page) const;

  // PhysicalWebListener implementation.
  void OnFound(const GURL& url) override;
  void OnLost(const GURL& url) override;
  void OnDistanceChanged(const GURL& url, double distance_estimate) override;

  // Fires the |OnSuggestionInvalidated| event for the suggestion corresponding
  // to the given |page_id| and deletes it from the dismissed IDs list, if
  // necessary.
  void InvalidateSuggestion(const std::string& page_id);

  void AppendToShownScannedUrls(
      const std::vector<ContentSuggestion>& suggestions);

  // Reads dismissed IDs from Prefs.
  std::set<std::string> ReadDismissedIDsFromPrefs() const;

  // Writes |dismissed_ids| into Prefs.
  void StoreDismissedIDsToPrefs(const std::set<std::string>& dismissed_ids);

  std::multimap<GURL, GURL> shown_resolved_urls_by_scanned_url_;
  CategoryStatus category_status_;
  const Category provided_category_;
  physical_web::PhysicalWebDataSource* physical_web_data_source_;
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(PhysicalWebPageSuggestionsProvider);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_PHYSICAL_WEB_PAGES_PHYSICAL_WEB_PAGE_SUGGESTIONS_PROVIDER_H_

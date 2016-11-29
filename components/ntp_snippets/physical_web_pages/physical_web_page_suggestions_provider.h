// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_PHYSICAL_WEB_PAGES_PHYSICAL_WEB_PAGE_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_PHYSICAL_WEB_PAGES_PHYSICAL_WEB_PAGE_SUGGESTIONS_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/values.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/physical_web/data_source/physical_web_listener.h"

namespace ntp_snippets {

// Provides content suggestions from the Physical Web Service.
class PhysicalWebPageSuggestionsProvider
    : public ContentSuggestionsProvider,
      public physical_web::PhysicalWebListener {
 public:
  PhysicalWebPageSuggestionsProvider(
      ContentSuggestionsProvider::Observer* observer,
      CategoryFactory* category_factory,
      physical_web::PhysicalWebDataSource* physical_web_data_source);
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

 private:
  friend class PhysicalWebPageSuggestionsProviderTest;

  // Updates the |category_status_| and notifies the |observer_|, if necessary.
  void NotifyStatusChanged(CategoryStatus new_status);

  // Manually requests all physical web pages and updates the suggestions.
  void FetchPhysicalWebPages();

  // Converts an Physical Web page to a ContentSuggestion.
  ContentSuggestion ConvertPhysicalWebPage(
      const base::DictionaryValue& page) const;

  // PhysicalWebListener implementation.
  void OnFound(const std::string& url) override;
  void OnLost(const std::string& url) override;
  void OnDistanceChanged(const std::string& url,
                         double distance_estimate) override;

  CategoryStatus category_status_;
  const Category provided_category_;
  physical_web::PhysicalWebDataSource* physical_web_data_source_;

  DISALLOW_COPY_AND_ASSIGN(PhysicalWebPageSuggestionsProvider);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_PHYSICAL_WEB_PAGES_PHYSICAL_WEB_PAGE_SUGGESTIONS_PROVIDER_H_

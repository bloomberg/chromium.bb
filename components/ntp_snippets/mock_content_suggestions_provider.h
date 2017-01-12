// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_MOCK_CONTENT_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_MOCK_CONTENT_SUGGESTIONS_PROVIDER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ntp_snippets {

// TODO(treib): This is a weird combination of a mock and a fake. Fix this.
class MockContentSuggestionsProvider : public ContentSuggestionsProvider {
 public:
  MockContentSuggestionsProvider(
      Observer* observer,
      const std::vector<Category>& provided_categories);
  ~MockContentSuggestionsProvider();

  void SetProvidedCategories(const std::vector<Category>& provided_categories);

  // Returns the status for |category|. The initial status in
  // CatgoryStatus::AVAILABLE. Will be updated on FireCategoryStatusChanged
  // events.
  CategoryStatus GetCategoryStatus(Category category) override;

  // Returns a hard-coded category info object.
  CategoryInfo GetCategoryInfo(Category category) override;

  // Forwards events to the underlying oberservers.
  // TODO(tschumann): This functionality does not belong here. Whoever injected
  // the observer into the constructor can as well notify the observer itself.
  void FireSuggestionsChanged(Category category,
                              std::vector<ContentSuggestion> suggestions);
  void FireCategoryStatusChanged(Category category, CategoryStatus new_status);
  void FireCategoryStatusChangedWithCurrentStatus(Category category);
  void FireSuggestionInvalidated(const ContentSuggestion::ID& suggestion_id);

  MOCK_METHOD3(ClearHistory,
               void(base::Time begin,
                    base::Time end,
                    const base::Callback<bool(const GURL& url)>& filter));
  MOCK_METHOD3(Fetch,
               void(const Category&,
                    const std::set<std::string>&,
                    const FetchDoneCallback&));
  MOCK_METHOD1(ClearCachedSuggestions, void(Category category));
  MOCK_METHOD2(GetDismissedSuggestionsForDebugging,
               void(Category category,
                    const DismissedSuggestionsCallback& callback));
  MOCK_METHOD1(ClearDismissedSuggestionsForDebugging, void(Category category));
  MOCK_METHOD1(DismissSuggestion,
               void(const ContentSuggestion::ID& suggestion_id));
  MOCK_METHOD2(FetchSuggestionImage,
               void(const ContentSuggestion::ID& suggestion_id,
                    const ImageFetchedCallback& callback));

 private:
  std::vector<Category> provided_categories_;
  std::map<int, CategoryStatus> statuses_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_MOCK_CONTENT_SUGGESTIONS_PROVIDER_H_

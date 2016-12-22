// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_SECTION_RANKERS_CLICK_BASED_SECTION_RANKER_H_
#define COMPONENTS_NTP_SNIPPETS_SECTION_RANKERS_CLICK_BASED_SECTION_RANKER_H_

#include <vector>

#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"

class PrefRegistrySimple;
class PrefService;

namespace ntp_snippets {

// An implementation of a CategoryRanker based on a number of clicks per
// category. Initial order is hardcoded, but sections with more clicks are moved
// to the top. The new remote categories must be registered using
// AppendCategoryIfNecessary. All other categories must be hardcoded in the
// initial order. The order and category usage data are persisted in prefs and
// reloaded on startup. TODO(crbug.com/675929): Remove unused categories from
// prefs.
class ClickBasedCategoryRanker : public CategoryRanker {
 public:
  explicit ClickBasedCategoryRanker(PrefService* pref_service);
  ~ClickBasedCategoryRanker() override;

  // CategoryRanker implementation.
  bool Compare(Category left, Category right) const override;
  void ClearHistory(base::Time begin, base::Time end) override;
  void AppendCategoryIfNecessary(Category category) override;
  void OnSuggestionOpened(Category category) override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns passing margin, i.e. a number of extra clicks required to move a
  // category upwards. For testing only.
  static int GetPassingMargin();

  // Returns number of top categories with extra margin (i.e. with increased
  // passing margin). For testing only.
  static int GetNumTopCategoriesWithExtraMargin();

 private:
  struct RankedCategory {
    Category category;
    int clicks;

    RankedCategory(Category category, int clicks);
  };

  int GetPositionPassingMargin(
      std::vector<RankedCategory>::const_iterator category_position) const;
  void RestoreDefaultOrder();
  void AppendKnownCategory(KnownCategories known_category);
  bool ReadOrderFromPrefs(std::vector<RankedCategory>* result_categories);
  void StoreOrderToPrefs(const std::vector<RankedCategory>& ordered_categories);
  std::vector<RankedCategory>::iterator FindCategory(Category category);
  bool ContainsCategory(Category category) const;

  std::vector<RankedCategory> ordered_categories_;
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(ClickBasedCategoryRanker);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_SECTION_RANKERS_CLICK_BASED_SECTION_RANKER_H_

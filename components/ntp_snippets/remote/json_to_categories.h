// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_HELPER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_HELPER_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"

namespace ntp_snippets {

struct FetchedCategory {
  Category category;
  CategoryInfo info;
  RemoteSuggestion::PtrVector suggestions;

  FetchedCategory(Category c, CategoryInfo&& info);
  FetchedCategory(FetchedCategory&&);
  ~FetchedCategory();
  FetchedCategory& operator=(FetchedCategory&&);
};

using FetchedCategoriesVector = std::vector<FetchedCategory>;

// Provides the CategoryInfo data for article suggestions. If |title| is
// nullopt, then the default, hard-coded title will be used.
CategoryInfo BuildArticleCategoryInfo(
    const base::Optional<base::string16>& title);

// Provides the CategoryInfo data for other remote suggestions.
CategoryInfo BuildRemoteCategoryInfo(const base::string16& title,
                                     bool allow_fetching_more_results);

bool JsonToCategories(const base::Value& parsed,
                      FetchedCategoriesVector* categories,
                      const base::Time& fetch_time);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_HELPER_H_

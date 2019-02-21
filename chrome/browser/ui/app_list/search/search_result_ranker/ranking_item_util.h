// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RANKING_ITEM_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RANKING_ITEM_UTIL_H_

namespace ash {
enum class SearchResultType;
}

class ChromeAppListItem;

namespace app_list {

// Warning: this enum may change, should not be serialised, and should not
// persist to logs.
enum class RankingItemType {
  kUnknown,
  kIgnored,
  kFile,
  kApp,
  kOmnibox,
  kArcAppShortcut
};

// Convert the enum used by |ChromeSearchResult|s into a |RankingItemType|.
RankingItemType RankingItemTypeFromSearchResultType(
    const ash::SearchResultType& type);

// Return the type of an |ChromeAppListItem|. We currently do not distinguish
// between different kinds of apps, and all |AppServiceAppItem|s are apps, so we
// trivially return |kApp|.
RankingItemType RankingItemTypeFromChromeAppListItem(
    const ChromeAppListItem& item);

};  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RANKING_ITEM_UTIL_H_

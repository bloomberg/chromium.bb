// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

namespace app_list {

using SearchResultType = ash::SearchResultType;

RankingItemType RankingItemTypeFromSearchResultType(
    const ash::SearchResultType& type) {
  switch (type) {
    case SearchResultType::kInstalledApp:
    case SearchResultType::kInternalApp:
      return RankingItemType::kApp;
    case SearchResultType::kOmnibox:
      return RankingItemType::kOmnibox;
    case SearchResultType::kLauncher:
      return RankingItemType::kFile;
    case SearchResultType::kUnknown:
    case SearchResultType::kPlayStoreApp:
    case SearchResultType::kInstantApp:
    case SearchResultType::kWebStoreApp:
    case SearchResultType::kAnswerCard:
    case SearchResultType::kPlayStoreReinstallApp:
    case SearchResultType::kWebStoreSearch:
      return RankingItemType::kIgnored;
  }
}

RankingItemType RankingItemTypeFromChromeAppListItem(
    const ChromeAppListItem& item) {
  return RankingItemType::kApp;
}

};  // namespace app_list

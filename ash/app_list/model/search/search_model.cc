// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/search_model.h"

namespace app_list {

SearchModel::SearchModel()
    : search_box_(std::make_unique<SearchBoxModel>()),
      results_(std::make_unique<SearchResults>()) {}

SearchModel::~SearchModel() {}

void SearchModel::SetTabletMode(bool started) {
  is_tablet_mode_ = started;
  search_box_->SetTabletMode(started);
}

std::vector<SearchResult*> SearchModel::FilterSearchResultsByDisplayType(
    SearchResults* results,
    SearchResult::DisplayType display_type,
    size_t max_results) {
  std::vector<SearchResult*> matches;
  for (size_t i = 0; i < results->item_count(); ++i) {
    SearchResult* item = results->GetItemAt(i);
    if (item->display_type() == display_type) {
      matches.push_back(item);
      if (matches.size() == max_results)
        break;
    }
  }
  return matches;
}

}  // namespace app_list

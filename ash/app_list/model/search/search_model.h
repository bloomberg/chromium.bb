// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_MODEL_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_model_export.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "ui/base/models/list_model.h"

namespace app_list {

class SearchBoxModel;

// A model of app list that holds two search related sub models:
// - SearchBoxModel: the model for SearchBoxView.
// - SearchResults: owning a list of SearchResult.
class APP_LIST_MODEL_EXPORT SearchModel {
 public:
  using SearchResults = ui::ListModel<SearchResult>;

  SearchModel();
  ~SearchModel();

  // Whether tablet mode is active. Controlled by AppListView.
  void SetTabletMode(bool started);
  bool tablet_mode() const { return is_tablet_mode_; }

  void SetSearchEngineIsGoogle(bool is_google) {
    search_engine_is_google_ = is_google;
  }
  bool search_engine_is_google() const { return search_engine_is_google_; }

  // Filters the given |results| by |display_type|. The returned list is
  // truncated to |max_results|.
  static std::vector<SearchResult*> FilterSearchResultsByDisplayType(
      SearchResults* results,
      SearchResult::DisplayType display_type,
      size_t max_results);

  SearchBoxModel* search_box() { return search_box_.get(); }
  SearchResults* results() { return results_.get(); }

  void PublishResults(std::vector<std::unique_ptr<SearchResult>> new_results);

  SearchResult* FindSearchResult(const std::string& id);

 private:
  std::unique_ptr<SearchBoxModel> search_box_;
  std::unique_ptr<SearchResults> results_;
  bool search_engine_is_google_ = false;
  // Whether tablet mode is active. Controlled by the AppListView.
  bool is_tablet_mode_ = false;

  DISALLOW_COPY_AND_ASSIGN(SearchModel);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_MODEL_H_

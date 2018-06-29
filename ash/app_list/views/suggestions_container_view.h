// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SUGGESTIONS_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_SUGGESTIONS_CONTAINER_VIEW_H_

#include <vector>

#include "ash/app_list/views/search_result_container_view.h"
#include "base/macros.h"

namespace app_list {

class AppListViewDelegate;
class ContentsView;
class PaginationModel;
class SearchResultTileItemView;

// A container that holds the suggested app tiles. If fullscreen app list is not
// enabled, it also holds the all apps button.
class SuggestionsContainerView : public SearchResultContainerView {
 public:
  SuggestionsContainerView(ContentsView* contents_view,
                           PaginationModel* pagination_model);
  ~SuggestionsContainerView() override;

  const std::vector<SearchResultTileItemView*>& tile_views() const {
    return search_result_tile_views_;
  }

  // Overridden from SearchResultContainerView:
  int DoUpdate() override;
  void NotifyFirstResultYIndex(int y_index) override;
  int GetYSize() override;
  SearchResultBaseView* GetFirstResultView() override;
  const char* GetClassName() const override;

 private:
  void CreateAppsGrid(int apps_num);

  ContentsView* contents_view_ = nullptr;
  AppListViewDelegate* view_delegate_ = nullptr;

  std::vector<SearchResultTileItemView*> search_result_tile_views_;

  PaginationModel* const pagination_model_;  // Owned by AppsGridView.

  DISALLOW_COPY_AND_ASSIGN(SuggestionsContainerView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SUGGESTIONS_CONTAINER_VIEW_H_

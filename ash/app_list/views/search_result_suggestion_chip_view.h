// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_SUGGESTION_CHIP_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_SUGGESTION_CHIP_VIEW_H_

#include <memory>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/suggestion_chip_view.h"
#include "base/macros.h"

namespace app_list {

class AppListViewDelegate;
class SearchResult;

// A chip view that displays a search result.
class APP_LIST_EXPORT SearchResultSuggestionChipView
    : public SearchResultBaseView {
 public:
  explicit SearchResultSuggestionChipView(AppListViewDelegate* view_delegate);
  ~SearchResultSuggestionChipView() override;

  SearchResult* result() { return item_; }
  void SetSearchResult(SearchResult* item);

  // SearchResultObserver:
  void OnMetadataChanged() override;
  void OnResultDestroying() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  void Layout() override;
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;

  SuggestionChipView* suggestion_chip_view() { return suggestion_chip_view_; }

 private:
  // Updates the suggestion chip view's title and icon.
  void UpdateSuggestionChipView();

  AppListViewDelegate* const view_delegate_;  // Owned by AppListView.

  // Owned by the model provided by the AppListViewDelegate.
  SearchResult* item_ = nullptr;

  // The view that actually shows the icon and title.
  SuggestionChipView* suggestion_chip_view_ = nullptr;

  base::WeakPtrFactory<SearchResultSuggestionChipView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultSuggestionChipView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_SUGGESTION_CHIP_VIEW_H_

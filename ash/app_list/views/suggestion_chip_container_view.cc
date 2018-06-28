// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/suggestion_chip_container_view.h"

#include <memory>

#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_result_suggestion_chip_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

// The spacing between chips.
constexpr int kChipSpacing = 8;

}  // namespace

SuggestionChipContainerView::SuggestionChipContainerView(
    ContentsView* contents_view)
    : contents_view_(contents_view) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  DCHECK(contents_view);
  view_delegate_ = contents_view_->GetAppListMainView()->view_delegate();

  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kChipSpacing));
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::MAIN_AXIS_ALIGNMENT_CENTER);
}

SuggestionChipContainerView::~SuggestionChipContainerView() = default;

int SuggestionChipContainerView::DoUpdate() {
  if (IgnoreUpdateAndLayout())
    return num_results();

  // Clear all current suggestion chips.
  for (size_t i = 0; i < suggestion_chip_views.size(); ++i)
    delete suggestion_chip_views[i];
  suggestion_chip_views.clear();

  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByDisplayType(
          results(), ash::SearchResultDisplayType::kRecommendation,
          kNumStartPageTiles);

  // Create a suggestion chip for each search result, but wait until layout to
  // add them as child views when we know this view's bounds.
  for (auto* result : display_results) {
    SearchResultSuggestionChipView* chip =
        new SearchResultSuggestionChipView(view_delegate_);
    chip->SetSearchResult(result);
    suggestion_chip_views.emplace_back(chip);
  }

  return num_results();
}

const char* SuggestionChipContainerView::GetClassName() const {
  return "SuggestionChipContainerView";
}

void SuggestionChipContainerView::Layout() {
  if (IgnoreUpdateAndLayout())
    return;

  // Only show the chips that fit in this view's contents bounds.
  RemoveAllChildViews(false /* delete_children */);
  int total_width = 0;
  const int max_width = GetContentsBounds().width();
  for (auto* chip : suggestion_chip_views) {
    const int chip_width = chip->GetPreferredSize().width();
    if (chip_width + total_width > max_width)
      break;
    AddChildView(chip);
    total_width += (total_width == 0 ? 0 : kChipSpacing) + chip_width;
  }

  views::View::Layout();
}

bool SuggestionChipContainerView::IgnoreUpdateAndLayout() const {
  // Ignore update and layout when this view is not shown.
  const ash::AppListState state = contents_view_->GetActiveState();
  return state != ash::AppListState::kStateStart &&
         state != ash::AppListState::kStateApps;
}

}  // namespace app_list

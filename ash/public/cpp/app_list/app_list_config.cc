// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_config.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "ui/gfx/color_palette.h"

namespace app_list {

AppListConfig::AppListConfig()
    : grid_tile_width_(120),
      grid_tile_height_(112),
      grid_tile_spacing_(0),
      grid_icon_dimension_(64),
      grid_icon_bottom_padding_(24),
      grid_title_top_padding_(82),
      grid_title_horizontal_padding_(0),
      grid_title_width_(grid_tile_width_),
      grid_title_color_(SK_ColorWHITE),
      grid_focus_dimension_(80),
      grid_focus_corner_radius_(12),
      search_tile_icon_dimension_(48),
      search_tile_badge_icon_dimension_(22),
      search_tile_badge_icon_offset_(5),
      search_list_icon_dimension_(20),
      search_list_badge_icon_dimension_(14),
      suggestion_chip_icon_dimension_(16),
      app_title_max_line_height_(20),
      app_title_font_(
          ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(1)),
      peeking_app_list_height_(284),
      search_box_closed_top_padding_(0),
      search_box_peeking_top_padding_(84),
      search_box_fullscreen_top_padding_(24),
      preferred_cols_(5),

      preferred_rows_(4),
      page_spacing_(48),
      expand_arrow_tile_height_(72),
      folder_bubble_radius_(44),
      folder_bubble_y_offset_(0),
      folder_icon_dimension_(72),
      folder_unclipped_icon_dimension_(88),
      folder_icon_radius_(36),
      folder_background_radius_(12),
      folder_bubble_color_(SkColorSetA(gfx::kGoogleGrey100, 0x7A)),
      item_icon_in_folder_icon_dimension_(32),
      folder_dropping_circle_radius_(44),
      folder_dropping_delay_(0),
      folder_background_color_(gfx::kGoogleGrey100),
      page_flip_zone_size_(20),
      grid_tile_spacing_in_folder_(8),
      // TODO(manucornet): Share the value with ShelfConstants and use
      // 48 when the new shelf UI is turned off.
      shelf_height_(56),
      blur_radius_(30) {}

AppListConfig::~AppListConfig() = default;

// static
const AppListConfig& AppListConfig::instance() {
  static const base::NoDestructor<AppListConfig> instance;
  return *instance;
}

int AppListConfig::GetPreferredIconDimension(
    ash::SearchResultDisplayType display_type) const {
  switch (display_type) {
    case ash::SearchResultDisplayType::kRecommendation:
      FALLTHROUGH;
    case ash::SearchResultDisplayType::kTile:
      return search_tile_icon_dimension_;
    case ash::SearchResultDisplayType::kList:
      return search_list_icon_dimension_;
    case ash::SearchResultDisplayType::kNone:  // Falls through.
    case ash::SearchResultDisplayType::kCard:
      return 0;
    case ash::SearchResultDisplayType::kLast:
      break;
  }
  NOTREACHED();
  return 0;
}

int AppListConfig::GetMaxNumOfItemsPerPage(int /* page */) const {
  // In new style launcher, the first row of first page is no longger suggestion
  // apps.
  return preferred_cols_ * preferred_rows_;
}

}  // namespace app_list

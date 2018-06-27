// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class FontList;
}

namespace app_list {

// Shared layout type information for app list. Use the instance() method to
// obtain the AppListConfig.
class ASH_PUBLIC_EXPORT AppListConfig {
 public:
  AppListConfig();
  ~AppListConfig();

  static const AppListConfig& instance();

  int grid_tile_width() const { return grid_tile_width_; }
  int grid_tile_height() const { return grid_tile_height_; }
  int grid_tile_spacing() const { return grid_tile_spacing_; }
  int grid_icon_dimension() const { return grid_icon_dimension_; }
  int grid_icon_top_padding() const { return grid_icon_top_padding_; }
  int grid_icon_title_spacing() const { return grid_icon_title_spacing_; }
  int grid_title_width() const { return grid_title_width_; }
  int grid_focus_dimension() const { return grid_focus_dimension_; }
  int grid_focus_corner_radius() const { return grid_focus_corner_radius_; }
  SkColor grid_title_color() const { return grid_title_color_; }
  int search_tile_icon_dimension() const { return search_tile_icon_dimension_; }
  int search_tile_badge_icon_dimension() const {
    return search_tile_badge_icon_dimension_;
  }
  int search_tile_badge_background_radius() const {
    return search_tile_badge_background_radius_;
  }
  int search_list_icon_dimension() const { return search_list_icon_dimension_; }
  int search_list_badge_icon_dimension() const {
    return search_list_badge_icon_dimension_;
  }
  int app_title_max_line_height() const { return app_title_max_line_height_; }
  const gfx::FontList& app_title_font() const { return app_title_font_; }

  gfx::Size grid_icon_size() const {
    return gfx::Size(grid_icon_dimension_, grid_icon_dimension_);
  }

  gfx::Size grid_focus_size() const {
    return gfx::Size(grid_focus_dimension_, grid_focus_dimension_);
  }

  gfx::Size search_tile_icon_size() const {
    return gfx::Size(search_tile_icon_dimension_, search_tile_icon_dimension_);
  }

  gfx::Size search_tile_badge_icon_size() const {
    return gfx::Size(search_tile_badge_icon_dimension_,
                     search_tile_badge_icon_dimension_);
  }

  gfx::Size search_list_icon_size() const {
    return gfx::Size(search_list_icon_dimension_, search_list_icon_dimension_);
  }

  gfx::Size search_list_badge_icon_size() const {
    return gfx::Size(search_list_badge_icon_dimension_,
                     search_list_badge_icon_dimension_);
  }

  // Returns the dimension at which a result's icon should be displayed.
  int GetPreferredIconDimension(
      ash::SearchResultDisplayType display_type) const;

 private:
  // The tile view's width and height of the item in apps grid view.
  int grid_tile_width_;
  int grid_tile_height_;

  // The spacing between tile views in apps grid view.
  int grid_tile_spacing_;

  // The icon dimension of tile views in apps grid view.
  int grid_icon_dimension_;

  // The icon top padding in tile views in apps grid view.
  int grid_icon_top_padding_;

  // The spacing between icon and title in tile views in apps grid view.
  int grid_icon_title_spacing_;

  // The title width and color of tile views in apps grid view.
  int grid_title_width_;
  SkColor grid_title_color_;

  // The focus dimension and corner radius of tile views in apps grid view.
  int grid_focus_dimension_;
  int grid_focus_corner_radius_;

  // The icon dimension of tile views in search result page view.
  int search_tile_icon_dimension_;

  // The badge icon dimension of tile views in search result page view.
  int search_tile_badge_icon_dimension_;

  // The badge background corner radius of tile views in search result page
  // view.
  int search_tile_badge_background_radius_;

  // The icon dimension of list views in search result page view.
  int search_list_icon_dimension_;

  // The badge background corner radius of list views in search result page
  // view.
  int search_list_badge_icon_dimension_;

  // The maximum line height for app title in app list.
  int app_title_max_line_height_;

  // The font for app title in app list.
  gfx::FontList app_title_font_;
};

}  // namespace app_list

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_

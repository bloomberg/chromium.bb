// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_config.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/gfx/color_palette.h"

namespace ash {

namespace {

// When any edge of the primary display is less than or equal to this threshold,
// dense shelf will be active.
const int kDenseShelfScreenSizeThreshold = 600;

}  // namespace

ShelfConfig::ShelfConfig()
    : is_dense_(false),
      is_app_list_visible_(false),
      shelf_button_icon_size_(44),
      shelf_button_icon_size_dense_(36),
      shelf_button_size_(56),
      shelf_button_size_dense_(48),
      shelf_button_spacing_(8),
      shelf_status_area_hit_region_padding_(4),
      shelf_status_area_hit_region_padding_dense_(2),
      app_icon_group_margin_(16),
      shelf_control_permanent_highlight_background_(
          SkColorSetA(SK_ColorWHITE, 26)),  // 10%
      shelf_focus_border_color_(gfx::kGoogleBlue300),
      workspace_area_visible_inset_(2),
      workspace_area_auto_hide_inset_(5),
      hidden_shelf_in_screen_portion_(3),
      shelf_default_base_color_(gfx::kGoogleGrey900),
      shelf_ink_drop_base_color_(SK_ColorWHITE),
      shelf_ink_drop_visible_opacity_(0.2f),
      shelf_icon_color_(SK_ColorWHITE),
      shelf_translucent_over_app_list_(51),      // 20%
      shelf_translucent_alpha_(189),             // 74%
      shelf_translucent_maximized_window_(254),  // ~100%
      shelf_translucent_color_darken_alpha_(178),
      shelf_opaque_color_darken_alpha_(178),
      status_indicator_offset_from_shelf_edge_(1),
      shelf_tooltip_preview_height_(128),
      shelf_tooltip_preview_max_width_(192),
      shelf_tooltip_preview_max_ratio_(1.5),     // = 3/2
      shelf_tooltip_preview_min_ratio_(0.666) {  // = 2/3
  UpdateIsDense();
}

ShelfConfig::~ShelfConfig() = default;

// static
ShelfConfig* ShelfConfig::Get() {
  return Shell::Get()->shelf_config();
}

void ShelfConfig::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ShelfConfig::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ShelfConfig::Init() {
  if (!chromeos::switches::ShouldShowShelfHotseat())
    return;

  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->app_list_controller()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

void ShelfConfig::Shutdown() {
  if (!chromeos::switches::ShouldShowShelfHotseat())
    return;

  display::Screen::GetScreen()->RemoveObserver(this);
  Shell::Get()->app_list_controller()->RemoveObserver(this);
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
}

void ShelfConfig::OnTabletModeStarted() {
  UpdateIsDense();
}

void ShelfConfig::OnTabletModeEnded() {
  UpdateIsDense();
}

void ShelfConfig::OnDisplayMetricsChanged(const display::Display& display,
                                          uint32_t changed_metrics) {
  UpdateIsDense();
}

void ShelfConfig::OnAppListVisibilityChanged(bool shown, int64_t display_id) {
  // Let's check that the app visibility mechanism isn't mis-firing, which
  // would lead to a lot of extraneous relayout work.
  DCHECK_NE(is_app_list_visible_, shown);

  is_app_list_visible_ = shown;
  for (auto& observer : observers_)
    observer.OnShelfConfigUpdated();
}

int ShelfConfig::shelf_size() const {
  // TODO(manucornet): Add unit tests to check that this returns the correct
  // sizes in various situations.

  // Before the hotseat redesign, the shelf always has the same size.
  if (!chromeos::switches::ShouldShowShelfHotseat())
    return 56;

  // In clamshell mode, the shelf always has the same size.
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
    return 48;

  if (is_dense_)
    return is_app_list_visible_ ? 48 : 36;
  else
    return is_app_list_visible_ ? 56 : 40;
}

int ShelfConfig::button_size() const {
  return is_dense_ ? shelf_button_size_dense_ : shelf_button_size_;
}

int ShelfConfig::button_spacing() const {
  return shelf_button_spacing_;
}

int ShelfConfig::button_icon_size() const {
  return is_dense_ ? shelf_button_icon_size_dense_ : shelf_button_icon_size_;
}

int ShelfConfig::control_size() const {
  if (!chromeos::switches::ShouldShowShelfHotseat())
    return 40;

  if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
    return 36;

  if (is_dense_)
    return is_app_list_visible_ ? 36 : 28;
  else
    return is_app_list_visible_ ? 40 : 30;
}

int ShelfConfig::control_border_radius() const {
  return control_size() / 2;
}

int ShelfConfig::overflow_button_margin() const {
  return (button_size() - control_size()) / 2;
}

int ShelfConfig::home_button_edge_spacing() const {
  return (shelf_size() - control_size()) / 2;
}

int ShelfConfig::status_area_hit_region_padding() const {
  return is_dense_ ? shelf_status_area_hit_region_padding_dense_
                   : shelf_status_area_hit_region_padding_;
}

void ShelfConfig::UpdateIsDense() {
  const gfx::Rect screen_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  const bool new_is_dense =
      chromeos::switches::ShouldShowShelfHotseat() &&
      (!Shell::Get()->tablet_mode_controller()->InTabletMode() ||
       (screen_size.width() <= kDenseShelfScreenSizeThreshold ||
        screen_size.height() <= kDenseShelfScreenSizeThreshold));
  if (new_is_dense == is_dense_)
    return;

  is_dense_ = new_is_dense;

  for (auto& observer : observers_)
    observer.OnShelfConfigUpdated();
}

}  // namespace ash

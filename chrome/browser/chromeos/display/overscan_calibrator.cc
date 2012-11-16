// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/overscan_calibrator.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"

namespace chromeos {

OverscanCalibrator::OverscanCalibrator(
    const gfx::Display& target_display, const gfx::Insets& initial_insets)
    : display_(target_display),
      insets_(initial_insets),
      initial_insets_(initial_insets),
      committed_(false) {
  // Undo the overscan calibration temporarily so that the user can see
  // dark boundary and current overscan region.
  ash::Shell::GetInstance()->display_controller()->SetOverscanInsets(
      display_.id(), gfx::Insets());

  aura::RootWindow* root = ash::Shell::GetInstance()->display_controller()->
      GetRootWindowForDisplayId(display_.id());
  ui::Layer* parent_layer = ash::Shell::GetContainer(
      root, ash::internal::kShellWindowId_OverlayContainer)->layer();

  calibration_layer_.reset(new ui::Layer());
  calibration_layer_->SetOpacity(0.5f);
  calibration_layer_->SetBounds(parent_layer->bounds());
  calibration_layer_->set_delegate(this);
  parent_layer->Add(calibration_layer_.get());
}

OverscanCalibrator::~OverscanCalibrator() {
  // Overscan calibration has finished without commit, so the display has to
  // be the original offset.
  if (!committed_) {
    ash::Shell::GetInstance()->display_controller()->SetOverscanInsets(
        display_.id(), initial_insets_);
  }
}

void OverscanCalibrator::Commit() {
  SetDisplayOverscan(display_, insets_);
  committed_ = true;
}

void OverscanCalibrator::UpdateInsets(const gfx::Insets& insets) {
  insets_ = insets;
  calibration_layer_->SchedulePaint(calibration_layer_->bounds());
}

void OverscanCalibrator::OnPaintLayer(gfx::Canvas* canvas) {
  static const SkColor transparent = SkColorSetARGB(0, 0, 0, 0);
  gfx::Rect full_bounds = calibration_layer_->bounds();
  gfx::Rect inner_bounds = full_bounds;
  inner_bounds.Inset(insets_);
  canvas->FillRect(full_bounds, SK_ColorBLACK);
  canvas->FillRect(inner_bounds, transparent, SkXfermode::kClear_Mode);
}

void OverscanCalibrator::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  // TODO(mukai): Cancel the overscan calibration when the device
  // configuration has changed.
}

base::Closure OverscanCalibrator::PrepareForLayerBoundsChange() {
  return base::Closure();
}

}  // namespace chromeos

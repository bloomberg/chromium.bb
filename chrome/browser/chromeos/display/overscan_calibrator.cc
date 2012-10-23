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
    : display_(target_display), insets_(initial_insets) {
  aura::RootWindow* root = ash::Shell::GetInstance()->display_controller()->
      GetRootWindowForDisplayId(display_.id());
  ui::Layer* parent_layer = ash::Shell::GetContainer(
      root, ash::internal::kShellWindowId_OverlayContainer)->layer();

  calibration_layer_.reset(new ui::Layer());
  calibration_layer_->SetOpacity(0.5f);
  calibration_layer_->SetBounds(parent_layer->bounds());
  calibration_layer_->set_delegate(this);
  parent_layer->Add(calibration_layer_.get());
  // The initial size of |inner_bounds_| has to be same as the root because the
  // root size already shrinks due to the current overscan settings.
  inner_bounds_ = parent_layer->bounds();
}

OverscanCalibrator::~OverscanCalibrator() {
}

void OverscanCalibrator::Commit() {
  SetDisplayOverscan(display_, insets_);
}

void OverscanCalibrator::UpdateInsets(const gfx::Insets& insets) {
  // Has to undo the old |insets_| in order to apply the new |insets|.
  inner_bounds_.Inset(-insets_);
  inner_bounds_.Inset(insets);
  insets_ = insets;
  calibration_layer_->SchedulePaint(calibration_layer_->bounds());
}

void OverscanCalibrator::OnPaintLayer(gfx::Canvas* canvas) {
  static const SkColor transparent = SkColorSetARGB(0, 0, 0, 0);
  canvas->FillRect(calibration_layer_->bounds(), SK_ColorBLACK);
  canvas->FillRect(inner_bounds_, transparent, SkXfermode::kClear_Mode);
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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/overscan_calibrator.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/callback.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"

namespace chromeos {
namespace {

// The opacity for the grey out borders.
const float kBorderOpacity = 0.5;

// The opacity for the arrows of the overscan calibration.
const float kArrowOpacity = 0.8;

// The height in pixel for the arrows to show the overscan calibration.
const int kCalibrationArrowHeight = 50;

// The gap between the boundary and calibration arrows.
const int kArrowGapWidth = 20;

// Draw the arrow for the overscan calibration to |canvas|.
void DrawTriangle(int x_offset,
                  int y_offset,
                  double rotation_degree,
                  gfx::Canvas* canvas) {
  // Draw triangular arrows.
  SkPaint content_paint;
  content_paint.setStyle(SkPaint::kFill_Style);
  content_paint.setColor(SkColorSetA(SK_ColorBLACK, kuint8max * kArrowOpacity));
  SkPaint border_paint;
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setColor(SkColorSetA(SK_ColorWHITE, kuint8max * kArrowOpacity));

  SkPath base_path;
  base_path.moveTo(0, SkIntToScalar(-kCalibrationArrowHeight));
  base_path.lineTo(SkIntToScalar(-kCalibrationArrowHeight), 0);
  base_path.lineTo(SkIntToScalar(kCalibrationArrowHeight), 0);
  base_path.close();

  SkPath path;
  gfx::Transform rotate_transform;
  rotate_transform.Rotate(rotation_degree);
  gfx::Transform move_transform;
  move_transform.Translate(x_offset, y_offset);
  rotate_transform.ConcatTransform(move_transform);
  base_path.transform(rotate_transform.matrix(), &path);

  canvas->DrawPath(path, content_paint);
  canvas->DrawPath(path, border_paint);
}

}  // namespace

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

  ash::internal::DisplayInfo info = ash::Shell::GetInstance()->
      display_manager()->GetDisplayInfo(display_.id());
  if (info.has_overscan()) {
    info.clear_has_custom_overscan_insets();
    info.UpdateDisplaySize();
    native_insets_ = info.overscan_insets_in_dip();
  }

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
  if (insets_ == native_insets_) {
    ash::Shell::GetInstance()->display_controller()->ClearCustomOverscanInsets(
        display_.id());
  } else {
    ash::Shell::GetInstance()->display_controller()->SetOverscanInsets(
        display_.id(), insets_);
  }
  committed_ = true;
}

void OverscanCalibrator::Reset() {
  if (!native_insets_.empty())
    insets_ = native_insets_;
  else
    insets_ = initial_insets_;

  calibration_layer_->SchedulePaint(calibration_layer_->bounds());
}

void OverscanCalibrator::UpdateInsets(const gfx::Insets& insets) {
  insets_.Set(std::max(insets.top(), 0),
              std::max(insets.left(), 0),
              std::max(insets.bottom(), 0),
              std::max(insets.right(), 0));
  calibration_layer_->SchedulePaint(calibration_layer_->bounds());
}

void OverscanCalibrator::OnPaintLayer(gfx::Canvas* canvas) {
  static const SkColor kTransparent = SkColorSetARGB(0, 0, 0, 0);
  gfx::Rect full_bounds = calibration_layer_->bounds();
  gfx::Rect inner_bounds = full_bounds;
  inner_bounds.Inset(insets_);
  canvas->FillRect(full_bounds, SK_ColorBLACK);
  canvas->FillRect(inner_bounds, kTransparent, SkXfermode::kClear_Mode);

  gfx::Point center = inner_bounds.CenterPoint();
  int vertical_offset = inner_bounds.height() / 2 - kArrowGapWidth;
  int horizontal_offset = inner_bounds.width() / 2 - kArrowGapWidth;

  DrawTriangle(center.x(), center.y() + vertical_offset, 0, canvas);
  DrawTriangle(center.x(), center.y() - vertical_offset, 180, canvas);
  DrawTriangle(center.x() - horizontal_offset, center.y(), 90, canvas);
  DrawTriangle(center.x() + horizontal_offset, center.y(), -90, canvas);
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

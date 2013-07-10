// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/image_cursors.h"

#include <float.h>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"

namespace ash {

const int kImageCursorIds[] = {
  ui::kCursorNull,
  ui::kCursorPointer,
  ui::kCursorNoDrop,
  ui::kCursorNotAllowed,
  ui::kCursorCopy,
  ui::kCursorHand,
  ui::kCursorMove,
  ui::kCursorNorthEastResize,
  ui::kCursorSouthWestResize,
  ui::kCursorSouthEastResize,
  ui::kCursorNorthWestResize,
  ui::kCursorNorthResize,
  ui::kCursorSouthResize,
  ui::kCursorEastResize,
  ui::kCursorWestResize,
  ui::kCursorIBeam,
  ui::kCursorAlias,
  ui::kCursorCell,
  ui::kCursorContextMenu,
  ui::kCursorCross,
  ui::kCursorHelp,
  ui::kCursorVerticalText,
  ui::kCursorZoomIn,
  ui::kCursorZoomOut,
  ui::kCursorRowResize,
  ui::kCursorColumnResize,
  ui::kCursorEastWestResize,
  ui::kCursorNorthSouthResize,
  ui::kCursorNorthEastSouthWestResize,
  ui::kCursorNorthWestSouthEastResize,
  ui::kCursorGrab,
  ui::kCursorGrabbing,
};

const int kAnimatedCursorIds[] = {
  ui::kCursorWait,
  ui::kCursorProgress
};

ImageCursors::ImageCursors() : scale_(1.f) {
}

ImageCursors::~ImageCursors() {
}

gfx::Display ImageCursors::GetDisplay() const {
  if (!cursor_loader_) {
    NOTREACHED();
    // Returning default on release build as it's not serious enough to crash
    // even if this ever happens.
    return gfx::Display();
  }
  return cursor_loader_->display();
}

bool ImageCursors::SetDisplay(const gfx::Display& display) {
  float device_scale_factor = display.device_scale_factor();
  if (!cursor_loader_) {
    cursor_loader_.reset(ui::CursorLoader::Create());
    cursor_loader_->set_scale(scale_);
  } else if (cursor_loader_->display().rotation() == display.rotation() &&
             cursor_loader_->display().device_scale_factor() ==
             device_scale_factor) {
    return false;
  }

  cursor_loader_->set_display(display);
  ReloadCursors();
  return true;
}

void ImageCursors::ReloadCursors() {
  const gfx::Display& display = cursor_loader_->display();
  float device_scale_factor = display.device_scale_factor();

  cursor_loader_->UnloadAll();

  for (size_t i = 0; i < arraysize(kImageCursorIds); ++i) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success = ui::GetCursorDataFor(kImageCursorIds[i],
                                        device_scale_factor,
                                        &resource_id,
                                        &hot_point);
    DCHECK(success);
    cursor_loader_->LoadImageCursor(kImageCursorIds[i], resource_id, hot_point);
  }
  for (size_t i = 0; i < arraysize(kAnimatedCursorIds); ++i) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success = ui::GetAnimatedCursorDataFor(kAnimatedCursorIds[i],
                                                device_scale_factor,
                                                &resource_id,
                                                &hot_point);
    DCHECK(success);
    cursor_loader_->LoadAnimatedCursor(kAnimatedCursorIds[i],
                                       resource_id,
                                       hot_point,
                                       ui::kAnimatedCursorFrameDelayMs);
  }
}

void ImageCursors::SetScale(float scale) {
  if (scale < FLT_EPSILON) {
    NOTREACHED() << "Scale must be bigger than 0.";
    scale = 1.0f;
  }

  scale_ = scale;

  if (cursor_loader_.get()) {
    cursor_loader_->set_scale(scale);
    ReloadCursors();
  }
}

void ImageCursors::SetPlatformCursor(gfx::NativeCursor* cursor) {
  cursor_loader_->SetPlatformCursor(cursor);
}

}  // namespace ash

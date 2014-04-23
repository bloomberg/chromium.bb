// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/image_cursors.h"

#include <float.h>

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
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

ImageCursors::ImageCursors() : cursor_set_(ui::CURSOR_SET_NORMAL) {
}

ImageCursors::~ImageCursors() {
}

float ImageCursors::GetScale() const {
  if (!cursor_loader_) {
    NOTREACHED();
    // Returning default on release build as it's not serious enough to crash
    // even if this ever happens.
    return 1.0f;
  }
  return cursor_loader_->scale();
}

gfx::Display::Rotation ImageCursors::GetRotation() const {
  if (!cursor_loader_) {
    NOTREACHED();
    // Returning default on release build as it's not serious enough to crash
    // even if this ever happens.
    return gfx::Display::ROTATE_0;
  }
  return cursor_loader_->rotation();
}

bool ImageCursors::SetDisplay(const gfx::Display& display) {
  DCHECK(display.is_valid());
  // Use the platform's device scale factor instead of display's
  // that might have been adjusted for UI scale.
  float scale_factor = Shell::GetInstance()->display_manager()->
      GetDisplayInfo(display.id()).device_scale_factor();

  if (!cursor_loader_) {
    cursor_loader_.reset(ui::CursorLoader::Create());
  } else if (cursor_loader_->rotation() == display.rotation() &&
             cursor_loader_->scale() == scale_factor) {
    return false;
  }

  cursor_loader_->set_rotation(display.rotation());
  cursor_loader_->set_scale(scale_factor);
  ReloadCursors();
  return true;
}

void ImageCursors::ReloadCursors() {
  float device_scale_factor = cursor_loader_->scale();

  cursor_loader_->UnloadAll();

  for (size_t i = 0; i < arraysize(kImageCursorIds); ++i) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success = ui::GetCursorDataFor(cursor_set_,
                                        kImageCursorIds[i],
                                        device_scale_factor,
                                        &resource_id,
                                        &hot_point);
    DCHECK(success);
    cursor_loader_->LoadImageCursor(kImageCursorIds[i], resource_id, hot_point);
  }
  for (size_t i = 0; i < arraysize(kAnimatedCursorIds); ++i) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success = ui::GetAnimatedCursorDataFor(cursor_set_,
                                                kAnimatedCursorIds[i],
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

void ImageCursors::SetCursorSet(ui::CursorSetType cursor_set) {
  if (cursor_set_ == cursor_set)
    return;

  cursor_set_ = cursor_set;

  if (cursor_loader_.get())
    ReloadCursors();
}

void ImageCursors::SetPlatformCursor(gfx::NativeCursor* cursor) {
  cursor_loader_->SetPlatformCursor(cursor);
}

}  // namespace ash

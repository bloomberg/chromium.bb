// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/image_cursors.h"

#include "base/logging.h"
#include "base/string16.h"
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

ImageCursors::ImageCursors() {
}

ImageCursors::~ImageCursors() {
}

gfx::Display ImageCursors::GetDisplay() const {
  if (!cursor_loader_.get()) {
    NOTREACHED();
    // Returning default on release build as it's not serious enough to crash
    // even if this ever happens.
    return gfx::Display();
  }
  return cursor_loader_->display();
}

bool ImageCursors::SetDisplay(const gfx::Display& display) {
  float device_scale_factor = display.device_scale_factor();
  if (!cursor_loader_.get()) {
    cursor_loader_.reset(ui::CursorLoader::Create());
  } else if (cursor_loader_->display().rotation() == display.rotation() &&
             cursor_loader_->display().device_scale_factor() ==
             device_scale_factor) {
    return false;
  }

  cursor_loader_->UnloadAll();
  cursor_loader_->set_display(display);

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
  return true;
}

void ImageCursors::SetPlatformCursor(gfx::NativeCursor* cursor) {
  cursor_loader_->SetPlatformCursor(cursor);
}

void ImageCursors::SetCursorResourceModule(const string16& module_name) {
  cursor_loader_->SetCursorResourceModule(module_name);
}

}  // namespace ash

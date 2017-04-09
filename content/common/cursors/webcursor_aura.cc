// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_util.h"

using blink::WebCursorInfo;

namespace content {

gfx::NativeCursor WebCursor::GetNativeCursor() {
  switch (type_) {
    case WebCursorInfo::kTypePointer:
      return ui::kCursorPointer;
    case WebCursorInfo::kTypeCross:
      return ui::kCursorCross;
    case WebCursorInfo::kTypeHand:
      return ui::kCursorHand;
    case WebCursorInfo::kTypeIBeam:
      return ui::kCursorIBeam;
    case WebCursorInfo::kTypeWait:
      return ui::kCursorWait;
    case WebCursorInfo::kTypeHelp:
      return ui::kCursorHelp;
    case WebCursorInfo::kTypeEastResize:
      return ui::kCursorEastResize;
    case WebCursorInfo::kTypeNorthResize:
      return ui::kCursorNorthResize;
    case WebCursorInfo::kTypeNorthEastResize:
      return ui::kCursorNorthEastResize;
    case WebCursorInfo::kTypeNorthWestResize:
      return ui::kCursorNorthWestResize;
    case WebCursorInfo::kTypeSouthResize:
      return ui::kCursorSouthResize;
    case WebCursorInfo::kTypeSouthEastResize:
      return ui::kCursorSouthEastResize;
    case WebCursorInfo::kTypeSouthWestResize:
      return ui::kCursorSouthWestResize;
    case WebCursorInfo::kTypeWestResize:
      return ui::kCursorWestResize;
    case WebCursorInfo::kTypeNorthSouthResize:
      return ui::kCursorNorthSouthResize;
    case WebCursorInfo::kTypeEastWestResize:
      return ui::kCursorEastWestResize;
    case WebCursorInfo::kTypeNorthEastSouthWestResize:
      return ui::kCursorNorthEastSouthWestResize;
    case WebCursorInfo::kTypeNorthWestSouthEastResize:
      return ui::kCursorNorthWestSouthEastResize;
    case WebCursorInfo::kTypeColumnResize:
      return ui::kCursorColumnResize;
    case WebCursorInfo::kTypeRowResize:
      return ui::kCursorRowResize;
    case WebCursorInfo::kTypeMiddlePanning:
      return ui::kCursorMiddlePanning;
    case WebCursorInfo::kTypeEastPanning:
      return ui::kCursorEastPanning;
    case WebCursorInfo::kTypeNorthPanning:
      return ui::kCursorNorthPanning;
    case WebCursorInfo::kTypeNorthEastPanning:
      return ui::kCursorNorthEastPanning;
    case WebCursorInfo::kTypeNorthWestPanning:
      return ui::kCursorNorthWestPanning;
    case WebCursorInfo::kTypeSouthPanning:
      return ui::kCursorSouthPanning;
    case WebCursorInfo::kTypeSouthEastPanning:
      return ui::kCursorSouthEastPanning;
    case WebCursorInfo::kTypeSouthWestPanning:
      return ui::kCursorSouthWestPanning;
    case WebCursorInfo::kTypeWestPanning:
      return ui::kCursorWestPanning;
    case WebCursorInfo::kTypeMove:
      return ui::kCursorMove;
    case WebCursorInfo::kTypeVerticalText:
      return ui::kCursorVerticalText;
    case WebCursorInfo::kTypeCell:
      return ui::kCursorCell;
    case WebCursorInfo::kTypeContextMenu:
      return ui::kCursorContextMenu;
    case WebCursorInfo::kTypeAlias:
      return ui::kCursorAlias;
    case WebCursorInfo::kTypeProgress:
      return ui::kCursorProgress;
    case WebCursorInfo::kTypeNoDrop:
      return ui::kCursorNoDrop;
    case WebCursorInfo::kTypeCopy:
      return ui::kCursorCopy;
    case WebCursorInfo::kTypeNone:
      return ui::kCursorNone;
    case WebCursorInfo::kTypeNotAllowed:
      return ui::kCursorNotAllowed;
    case WebCursorInfo::kTypeZoomIn:
      return ui::kCursorZoomIn;
    case WebCursorInfo::kTypeZoomOut:
      return ui::kCursorZoomOut;
    case WebCursorInfo::kTypeGrab:
      return ui::kCursorGrab;
    case WebCursorInfo::kTypeGrabbing:
      return ui::kCursorGrabbing;
    case WebCursorInfo::kTypeCustom: {
      ui::Cursor cursor(ui::kCursorCustom);
      cursor.SetPlatformCursor(GetPlatformCursor());
      return cursor;
    }
    default:
      NOTREACHED();
      return gfx::kNullCursor;
  }
}

float WebCursor::GetCursorScaleFactor() {
  DCHECK(custom_scale_ != 0);
  return device_scale_factor_ / custom_scale_;
}

void WebCursor::CreateScaledBitmapAndHotspotFromCustomData(
    SkBitmap* bitmap,
    gfx::Point* hotspot) {
  if (custom_data_.empty())
    return;
  ImageFromCustomData(bitmap);
  *hotspot = hotspot_;
  ui::ScaleAndRotateCursorBitmapAndHotpoint(
      GetCursorScaleFactor(), display::Display::ROTATE_0, bitmap, hotspot);
}

// ozone has its own SetDisplayInfo that takes rotation into account
#if !defined(USE_OZONE)
void WebCursor::SetDisplayInfo(const display::Display& display) {
  if (device_scale_factor_ == display.device_scale_factor())
    return;

  device_scale_factor_ = display.device_scale_factor();
  CleanupPlatformData();
}
#endif

}  // namespace content

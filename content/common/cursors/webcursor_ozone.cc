// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/ozone/public/cursor_factory_ozone.h"

namespace {
const float kMaxCursorWidth = 64.f;
const float kMaxCursorHeight = 64.f;
}

namespace content {

ui::PlatformCursor WebCursor::GetPlatformCursor() {
  if (platform_cursor_)
    return platform_cursor_;

  SkBitmap bitmap;
  ImageFromCustomData(&bitmap);
  gfx::Point hotspot = hotspot_;

  // TODO(spang): Consider allowing larger cursors if the hardware supports it.
  float scale = device_scale_factor_ / custom_scale_;
  scale = std::min(scale, kMaxCursorWidth / bitmap.width());
  scale = std::min(scale, kMaxCursorHeight / bitmap.height());

  ui::ScaleAndRotateCursorBitmapAndHotpoint(scale, rotation_, &bitmap,
                                            &hotspot);

  platform_cursor_ =
      ui::CursorFactoryOzone::GetInstance()->CreateImageCursor(bitmap, hotspot);
  return platform_cursor_;
}

void WebCursor::SetDisplayInfo(const gfx::Display& display) {
  if (rotation_ == display.rotation() &&
      device_scale_factor_ == display.device_scale_factor())
    return;

  device_scale_factor_ = display.device_scale_factor();
  rotation_ = display.rotation();
  if (platform_cursor_)
    ui::CursorFactoryOzone::GetInstance()->UnrefImageCursor(platform_cursor_);
  platform_cursor_ = NULL;
  // It is not necessary to recreate platform_cursor_ yet, since it will be
  // recreated on demand when GetPlatformCursor is called.
}

void WebCursor::InitPlatformData() {
  platform_cursor_ = NULL;
  device_scale_factor_ = 1.f;
  rotation_ = gfx::Display::ROTATE_0;
}

bool WebCursor::SerializePlatformData(base::Pickle* pickle) const {
  return true;
}

bool WebCursor::DeserializePlatformData(base::PickleIterator* iter) {
  return true;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  if (platform_cursor_) {
    ui::CursorFactoryOzone::GetInstance()->UnrefImageCursor(platform_cursor_);
    platform_cursor_ = NULL;
  }
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  if (platform_cursor_)
    ui::CursorFactoryOzone::GetInstance()->UnrefImageCursor(platform_cursor_);
  platform_cursor_ = other.platform_cursor_;
  if (platform_cursor_)
    ui::CursorFactoryOzone::GetInstance()->RefImageCursor(platform_cursor_);

  device_scale_factor_ = other.device_scale_factor_;
}

}  // namespace content

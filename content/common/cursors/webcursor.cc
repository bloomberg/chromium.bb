// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <algorithm>

#include "base/logging.h"
#include "base/pickle.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_image.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

using blink::WebCursorInfo;

constexpr int kMaxSize = 1024;

namespace content {

namespace {

// Checks for a reasonable value of a |CursorInfo::image_scale_factor|.
bool IsReasonableScale(float scale) {
  return scale >= 0.01f && scale <= 100.f;
}

}  // namespace

WebCursor::~WebCursor() {
  CleanupPlatformData();
}

WebCursor::WebCursor(const CursorInfo& info) : info_(info) {
  DCHECK(IsReasonableScale(info.image_scale_factor)) << info.image_scale_factor;
  DCHECK_LE(info.custom_image.width(), kMaxSize);
  DCHECK_LE(info.custom_image.height(), kMaxSize);
  DCHECK_LE(info.custom_image.width() / info.image_scale_factor, kMaxSize);
  DCHECK_LE(info.custom_image.height() / info.image_scale_factor, kMaxSize);
  ClampHotspot();
}

WebCursor::WebCursor(const WebCursor& other) : info_(other.info_) {
  CopyPlatformData(other);
}

bool WebCursor::Deserialize(const base::Pickle* m, base::PickleIterator* iter) {
  // Leave |this| unmodified unless deserialization is successful.
  int type = 0, hotspot_x = 0, hotspot_y = 0;
  float scale = 1.f;
  SkBitmap bitmap;
  if (!iter->ReadInt(&type))
    return false;

  if (type == WebCursorInfo::kTypeCustom) {
    if (!iter->ReadInt(&hotspot_x) || !iter->ReadInt(&hotspot_y) ||
        !iter->ReadFloat(&scale) || !IsReasonableScale(scale)) {
      return false;
    }

    if (!IPC::ParamTraits<SkBitmap>::Read(m, iter, &bitmap))
      return false;

    if (bitmap.width() > kMaxSize || bitmap.height() > kMaxSize ||
        bitmap.width() / scale > kMaxSize ||
        bitmap.height() / scale > kMaxSize) {
      return false;
    }
  }

  info_.type = static_cast<WebCursorInfo::Type>(type);
  info_.custom_image = bitmap;
  info_.hotspot = gfx::Point(hotspot_x, hotspot_y);
  info_.image_scale_factor = scale;
  ClampHotspot();
  return true;
}

void WebCursor::Serialize(base::Pickle* pickle) const {
  pickle->WriteInt(info_.type);
  if (info_.type == WebCursorInfo::kTypeCustom) {
    pickle->WriteInt(info_.hotspot.x());
    pickle->WriteInt(info_.hotspot.y());
    pickle->WriteFloat(info_.image_scale_factor);
    IPC::ParamTraits<SkBitmap>::Write(pickle, info_.custom_image);
  }
}

bool WebCursor::operator==(const WebCursor& other) const {
  return info_ == other.info_ &&
#if defined(USE_AURA) || defined(USE_OZONE)
         rotation_ == other.rotation_ &&
#endif
         IsPlatformDataEqual(other);
}

bool WebCursor::operator!=(const WebCursor& other) const {
  return !(*this == other);
}

void WebCursor::ClampHotspot() {
  if (info_.type != WebCursorInfo::kTypeCustom)
    return;

  // Clamp the hotspot to the custom image's dimensions.
  info_.hotspot.set_x(
      std::max(0, std::min(info_.custom_image.width() - 1, info_.hotspot.x())));
  info_.hotspot.set_y(std::max(
      0, std::min(info_.custom_image.height() - 1, info_.hotspot.y())));
}

}  // namespace content

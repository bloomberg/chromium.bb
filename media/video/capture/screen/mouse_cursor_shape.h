// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_MOUSE_CURSOR_SHAPE_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_MOUSE_CURSOR_SHAPE_H_

#include <string>

#include "base/basictypes.h"
#include "media/base/media_export.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace media {

// Type used to return mouse cursor shape from video capturers.
struct MEDIA_EXPORT MouseCursorShape {
  // Size of the cursor in screen pixels.
  webrtc::DesktopSize size;

  // Coordinates of the cursor hotspot relative to upper-left corner.
  webrtc::DesktopVector hotspot;

  // Cursor pixmap data in 32-bit BGRA format.
  std::string data;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_MOUSE_CURSOR_SHAPE_H_

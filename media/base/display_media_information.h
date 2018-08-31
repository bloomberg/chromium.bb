// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DISPLAY_MEDIA_INFORMATION_H_
#define MEDIA_BASE_DISPLAY_MEDIA_INFORMATION_H_

#include "base/logging.h"
#include "base/macros.h"
#include "media/base/media_export.h"

namespace media {

// Following the enums listed at
// https://w3c.github.io/mediacapture-screen-share/#extensions-to-mediatrackconstraintset.

// Describes the different types of display surface.
enum class DisplayCaptureSurfaceType {
  MONITOR,
  WINDOW,
  APPLICATION,
  BROWSER,
  LAST = BROWSER
};

// Describes the conditions under which the cursor is captured.
enum class CursorCaptureType { NEVER, ALWAYS, MOTION, LAST = MOTION };

struct MEDIA_EXPORT DisplayMediaInformation {
  DisplayMediaInformation();
  DisplayMediaInformation(DisplayCaptureSurfaceType display_surface,
                          bool logical_surface,
                          CursorCaptureType cursor);

  DisplayCaptureSurfaceType display_surface =
      DisplayCaptureSurfaceType::MONITOR;
  bool logical_surface = false;
  CursorCaptureType cursor = CursorCaptureType::NEVER;
};

}  // namespace media

#endif  // MEDIA_BASE_DISPLAY_MEDIA_INFORMATION_H_

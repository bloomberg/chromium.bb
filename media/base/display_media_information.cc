// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/display_media_information.h"

namespace media {

DisplayMediaInformation::DisplayMediaInformation() = default;

DisplayMediaInformation::DisplayMediaInformation(
    DisplayCaptureSurfaceType display_surface,
    bool logical_surface,
    CursorCaptureType cursor)
    : display_surface(display_surface),
      logical_surface(logical_surface),
      cursor(cursor) {}

}  // namespace media

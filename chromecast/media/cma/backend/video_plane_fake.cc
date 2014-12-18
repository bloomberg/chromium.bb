// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video_plane_fake.h"

#include "base/logging.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {

VideoPlaneFake::VideoPlaneFake() {
}

VideoPlaneFake::~VideoPlaneFake() {
}

gfx::Size VideoPlaneFake::GetScreenResolution() {
  return gfx::Size(1920, 1080);
}

void VideoPlaneFake::SetGeometry(const gfx::QuadF& quad,
                                 CoordinateType coordinate_type) {
  // Nothing to be done.
}

void VideoPlaneFake::OnScreenResolutionChanged(const gfx::Size& screen_res) {
}

}  // namespace media
}  // namespace chromecast

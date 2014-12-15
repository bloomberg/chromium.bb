// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video_plane_fake.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {

// static
VideoPlaneFake* VideoPlaneFake::GetInstance() {
  return Singleton<VideoPlaneFake>::get();
}

VideoPlaneFake::VideoPlaneFake() {
}

VideoPlaneFake::~VideoPlaneFake() {
}

gfx::Size VideoPlaneFake::GetVideoPlaneResolution() {
  return gfx::Size(1920, 1080);
}

void VideoPlaneFake::SetGeometry(const gfx::QuadF& quad,
                                 CoordinateType coordinate_type) {
  // Nothing to be done.
}

}  // namespace media
}  // namespace chromecast

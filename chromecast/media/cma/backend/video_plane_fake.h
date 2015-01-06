// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PLANE_FAKE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PLANE_FAKE_H_

#include "base/macros.h"
#include "chromecast/media/cma/backend/video_plane.h"

namespace chromecast {
namespace media {

class VideoPlaneFake : public VideoPlane {
 public:
  VideoPlaneFake();
  ~VideoPlaneFake() override;

  // VideoPlane implementation.
  gfx::Size GetScreenResolution() override;
  void SetGeometry(const gfx::QuadF& quad,
                   CoordinateType coordinate_type) override;
  void OnScreenResolutionChanged(const gfx::Size& screen_res) override;

  DISALLOW_COPY_AND_ASSIGN(VideoPlaneFake);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PLANE_FAKE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PLANE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PLANE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace gfx {
class QuadF;
class Size;
}

namespace chromecast {
namespace media {

class VideoPlane {
 public:
  enum CoordinateType {
    // Graphics plane as coordinate type.
    COORDINATE_TYPE_GRAPHICS_PLANE = 0,
    // Output display screen as coordinate type.
    COORDINATE_TYPE_SCREEN_RESOLUTION = 1,
  };

  VideoPlane();
  virtual ~VideoPlane();

  // Gets output screen resolution.
  virtual gfx::Size GetScreenResolution() = 0;

  // Updates the video plane geometry.
  // |quad.p1()| corresponds to the top left of the original video,
  // |quad.p2()| to the top right of the original video,
  // and so on.
  // Depending on the underlying hardware, the exact geometry
  // might not be honored.
  // |coordinate_type| indicates what coordinate type |quad| refers to.
  virtual void SetGeometry(const gfx::QuadF& quad,
                           CoordinateType coordinate_type) = 0;

  // Should be invoked whenever screen resolution changes (e.g. when a device is
  // plugged into a new HDMI port and a new HDMI EDID is received).
  // VideoPlane should reposition itself according to the new screen resolution.
  virtual void OnScreenResolutionChanged(const gfx::Size& screen_res) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoPlane);
};

// Factory to create a VideoPlane.
scoped_ptr<VideoPlane> CreateVideoPlane();

// Global accessor to the video plane.
VideoPlane* GetVideoPlane();

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PLANE_H_

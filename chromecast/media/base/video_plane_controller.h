// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_VIDEO_PLANE_CONTROLLER_H_
#define CHROMECAST_MEDIA_VIDEO_PLANE_CONTROLLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/video_plane.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {
namespace media {

// Provides main interface for setting video plane geometry.  All callsites
// should use this over VideoPlane::SetGeometry.  Reasons for this:
// * provides conversion between graphics plane coordinates and screen
//   resolution coordinates
// * updates VideoPlane when graphics plane or screen resolution changes
// * handles threading correctly (posting SetGeometry to media thread).
// * coalesces multiple calls in short space of time to prevent flooding the
//   media thread with SetGeometry calls (which are expensive on many
//   platforms).
// up to be informed of certain resolution changes.
class VideoPlaneController {
 public:
  static VideoPlaneController* GetInstance();

  // Sets the video plane geometry (forwards to VideoPlane::SetGeometry)
  // in *graphics plane coordinates*.
  //  * This should be called on UI thread (hopping to media thread is handled
  //    internally).
  void SetGeometry(const RectF& display_rect,
                   media::VideoPlane::Transform transform);

  // Handles a change in physical screen resolution.  This must be called when
  // the final output resolution (HDMI signal or panel resolution) changes.
  void OnDeviceResolutionChanged(const Size& resolution);

  // Handles a change in graphics hardware plane resolution.  This must be
  // called when the hardware graphics plane resolution changes (same resolution
  // as gfx::Screen).
  void OnGraphicsPlaneResolutionChanged(const Size& resolution);

 private:
  class RateLimitedSetVideoPlaneGeometry;
  friend struct base::DefaultSingletonTraits<VideoPlaneController>;

  VideoPlaneController();
  ~VideoPlaneController();

  // Current resolutions
  Size output_res_;
  Size graphics_res_;

  // Saved video plane parameters (in graphics plane coordinates)
  // for use when screen resolution changes.
  bool have_video_plane_geometry_;
  RectF video_plane_display_rect_;
  VideoPlane::Transform video_plane_transform_;

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<RateLimitedSetVideoPlaneGeometry> video_plane_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(VideoPlaneController);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_VIDEO_PLANE_CONTROLLER_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_VIDEO_PLANE_CONTROLLER_H_
#define CHROMECAST_MEDIA_VIDEO_PLANE_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_checker.h"
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
// The class collects/caches the data it needs before it can start operating.
// This means SetGeometry, SetDeviceResolution, and SetGraphicsPlaneResolution
// need to be called at least once in any order before the class starts making
// calls to VideoPlane::SetGeometry. All calls to these methods beforehand just
// set/update the cached parameters.
// All calls to public methods should be from the same thread.
class VideoPlaneController {
 public:
  static VideoPlaneController* GetInstance();

  // Sets the video plane geometry (forwards to VideoPlane::SetGeometry)
  // in *graphics plane coordinates*.
  //  * This should be called on UI thread (hopping to media thread is handled
  //    internally).
  // If there is no change to video plane parameters from the last call to this
  // method, it is a no-op.
  void SetGeometry(const RectF& display_rect, VideoPlane::Transform transform);

  // Sets physical screen resolution.  This must be called at least once when
  // the final output resolution (HDMI signal or panel resolution) is known,
  // then later when it changes. If there is no change to the device resolution
  // from the last call to this method, it is a no-op.
  void SetDeviceResolution(const Size& resolution);

  // Sets graphics hardware plane resolution.  This must be called at least once
  // when the hardware graphics plane resolution (same resolution as
  // gfx::Screen) is known, then later when it changes. If there is no change to
  // the graphics plane resolution from the last call to this method, it is a
  // no-op.
  void SetGraphicsPlaneResolution(const Size& resolution);

 private:
  class RateLimitedSetVideoPlaneGeometry;
  friend struct base::DefaultSingletonTraits<VideoPlaneController>;

  VideoPlaneController();
  ~VideoPlaneController();

  // Check if HaveDataForSetGeometry. If not, this method is a no-op. Otherwise
  // it scales the display rect from graphics to device resolution coordinates.
  // Then posts task to media thread for VideoPlane::SetGeometry.
  void MaybeRunSetGeometry();
  // Checks if all data has been collected to make calls to
  // VideoPlane::SetGeometry.
  bool HaveDataForSetGeometry() const;

  // Current resolutions
  bool have_output_res_;
  bool have_graphics_res_;
  Size output_res_;
  Size graphics_res_;

  // Saved video plane parameters (in graphics plane coordinates)
  // for use when screen resolution changes.
  bool have_video_plane_geometry_;
  RectF video_plane_display_rect_;
  VideoPlane::Transform video_plane_transform_;

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<RateLimitedSetVideoPlaneGeometry> video_plane_wrapper_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VideoPlaneController);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_VIDEO_PLANE_CONTROLLER_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_VIDEO_PLANE_H_
#define CHROMECAST_PUBLIC_VIDEO_PLANE_H_

namespace chromecast {
struct RectF;
struct Size;

namespace media {

class VideoPlane {
 public:
  // List of possible hardware transforms that can be applied to video.
  // Rotations are anticlockwise.
  enum Transform {
    TRANSFORM_NONE,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270,
    FLIP_HORIZONTAL,
    FLIP_VERTICAL,
  };

  enum CoordinateType {
    // Graphics plane as coordinate type.
    COORDINATE_TYPE_GRAPHICS_PLANE = 0,
    // Output display screen as coordinate type.
    COORDINATE_TYPE_SCREEN_RESOLUTION = 1,
  };

  virtual ~VideoPlane() {}

  // Gets output screen resolution.
  virtual Size GetScreenResolution() = 0;

  // Updates the video plane geometry.
  // |display_rect| specifies the rectangle that the video should occupy.
  // |coordinate_type| gives the coordinate space of |display_rect|.
  // |transform| specifies how the video should be transformed within that
  // rectangle.
  virtual void SetGeometry(const RectF& display_rect,
                           CoordinateType coordinate_type,
                           Transform transform) = 0;

  // Notifies VideoPlane that screen resolution has changed.
  virtual void OnScreenResolutionChanged(const Size& screen_res) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_VIDEO_PLANE_H_

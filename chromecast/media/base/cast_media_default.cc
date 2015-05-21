// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/video_plane.h"

namespace chromecast {
namespace media {
namespace {

class DefaultVideoPlane : public VideoPlane {
 public:
  ~DefaultVideoPlane() override {}

  Size GetScreenResolution() override {
    return Size(1920, 1080);
  }

  void SetGeometry(const RectF& display_rect,
                   CoordinateType coordinate_type,
                   Transform transform) override {}

  void OnScreenResolutionChanged(const Size& screen_res) override {}
};

DefaultVideoPlane* g_video_plane = nullptr;

}  // namespace

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {
  g_video_plane = new DefaultVideoPlane();
}

void CastMediaShlib::Finalize() {
  delete g_video_plane;
  g_video_plane = nullptr;
}

VideoPlane* CastMediaShlib::GetVideoPlane() {
  return g_video_plane;
}

}  // namespace media
}  // namespace chromecast

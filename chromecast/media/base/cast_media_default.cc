// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_default.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media_codec_support_shlib.h"
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

MediaPipelineBackend* CastMediaShlib::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  return new MediaPipelineBackendDefault(params);
}

MediaCodecSupportShlib::CodecSupport MediaCodecSupportShlib::IsSupported(
    const std::string& codec) {
  return kDefault;
}

}  // namespace media
}  // namespace chromecast

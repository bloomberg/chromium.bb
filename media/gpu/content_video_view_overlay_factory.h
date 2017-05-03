// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CONTENT_VIDEO_VIEW_OVERLAY_FACTORY_H_
#define MEDIA_GPU_CONTENT_VIDEO_VIEW_OVERLAY_FACTORY_H_

#include "media/base/android/android_overlay_factory.h"

namespace media {

// Factory for CVV-based overlays.  One needs this only to hide the factory
// impl from AVDA.
class ContentVideoViewOverlayFactory : public AndroidOverlayFactory {
 public:
  // |surface_id| must not be kNoSurfaceID.
  ContentVideoViewOverlayFactory(int32_t surface_id);
  ~ContentVideoViewOverlayFactory() override;

  std::unique_ptr<AndroidOverlay> CreateOverlay(
      const AndroidOverlay::Config& config) override;

 private:
  int32_t surface_id_;

  DISALLOW_COPY_AND_ASSIGN(ContentVideoViewOverlayFactory);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CONTENT_VIDEO_VIEW_OVERLAY_FACTORY_H_

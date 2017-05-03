// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/content_video_view_overlay_factory.h"

#include "base/memory/ptr_util.h"
#include "media/base/surface_manager.h"
#include "media/gpu/content_video_view_overlay.h"

namespace media {

ContentVideoViewOverlayFactory::ContentVideoViewOverlayFactory(
    int32_t surface_id)
    : surface_id_(surface_id) {
  DCHECK_NE(surface_id_, SurfaceManager::kNoSurfaceID);
}

ContentVideoViewOverlayFactory::~ContentVideoViewOverlayFactory() {}

std::unique_ptr<AndroidOverlay> ContentVideoViewOverlayFactory::CreateOverlay(
    const AndroidOverlay::Config& config) {
  return base::MakeUnique<ContentVideoViewOverlay>(surface_id_, config);
}

}  // namespace media

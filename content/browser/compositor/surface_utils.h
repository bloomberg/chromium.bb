// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_
#define CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_

#include <memory>

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/content_export.h"
#include "content/public/browser/readback_types.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class CopyOutputResult;
class FrameSinkManager;
}  // namespace cc

namespace viz {
class HostFrameSinkManager;
class FrameSinkManagerImpl;
}

namespace content {

CONTENT_EXPORT viz::FrameSinkId AllocateFrameSinkId();

CONTENT_EXPORT cc::FrameSinkManager* GetFrameSinkManager();

CONTENT_EXPORT viz::HostFrameSinkManager* GetHostFrameSinkManager();

void CopyFromCompositingSurfaceHasResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType color_type,
    const ReadbackRequestCallback& callback,
    std::unique_ptr<cc::CopyOutputResult> result);

namespace surface_utils {

CONTENT_EXPORT void ConnectWithInProcessFrameSinkManager(
    viz::HostFrameSinkManager* host,
    viz::FrameSinkManagerImpl* manager);

}  // namespace surface_utils

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_

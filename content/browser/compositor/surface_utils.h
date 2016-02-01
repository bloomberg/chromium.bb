// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_
#define CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/readback_types.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class CopyOutputResult;
class SurfaceIdAllocator;
class SurfaceManager;
}  // namespace cc

namespace content {

scoped_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator();

cc::SurfaceManager* GetSurfaceManager();

void CopyFromCompositingSurfaceHasResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType color_type,
    const ReadbackRequestCallback& callback,
    scoped_ptr<cc::CopyOutputResult> result);

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_

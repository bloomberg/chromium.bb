// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_
#define CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_

#include "base/memory/scoped_ptr.h"

namespace cc {
class SurfaceIdAllocator;
class SurfaceManager;
}  // namespace cc

namespace content {

scoped_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator();

cc::SurfaceManager* GetSurfaceManager();

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_

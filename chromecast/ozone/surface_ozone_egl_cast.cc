// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ozone/surface_ozone_egl_cast.h"

#include "chromecast/ozone/surface_factory_cast.h"
#include "ui/gfx/vsync_provider.h"

namespace chromecast {
namespace ozone {

SurfaceOzoneEglCast::~SurfaceOzoneEglCast() {
  parent_->ChildDestroyed();
}

intptr_t SurfaceOzoneEglCast::GetNativeWindow() {
  return reinterpret_cast<intptr_t>(parent_->GetNativeWindow());
}

bool SurfaceOzoneEglCast::OnSwapBuffers() {
  return true;
}

bool SurfaceOzoneEglCast::OnSwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  callback.Run();
  return true;
}

bool SurfaceOzoneEglCast::ResizeNativeWindow(const gfx::Size& viewport_size) {
  return parent_->ResizeDisplay(viewport_size);
}

scoped_ptr<gfx::VSyncProvider> SurfaceOzoneEglCast::CreateVSyncProvider() {
  return scoped_ptr<gfx::VSyncProvider>();
}

}  // namespace ozone
}  // namespace chromecast

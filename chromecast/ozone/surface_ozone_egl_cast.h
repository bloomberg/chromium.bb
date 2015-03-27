// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_OZONE_SURFACE_OZONE_EGL_CAST_H_
#define CHROMECAST_OZONE_SURFACE_OZONE_EGL_CAST_H_

#include "ui/ozone/public/surface_ozone_egl.h"

namespace chromecast {
namespace ozone {

class SurfaceFactoryCast;

// EGL surface wrapper for OzonePlatformCast.
class SurfaceOzoneEglCast : public ui::SurfaceOzoneEGL {
 public:
  explicit SurfaceOzoneEglCast(SurfaceFactoryCast* parent) : parent_(parent) {}
  ~SurfaceOzoneEglCast() override;

  // SurfaceOzoneEGL implementation:
  intptr_t GetNativeWindow() override;
  bool OnSwapBuffers() override;
  bool OnSwapBuffersAsync(const SwapCompletionCallback& callback) override;
  bool ResizeNativeWindow(const gfx::Size& viewport_size) override;
  scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;

 private:
  SurfaceFactoryCast* parent_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceOzoneEglCast);
};

}  // namespace ozone
}  // namespace chromecast

#endif  // CHROMECAST_OZONE_SURFACE_OZONE_EGL_CAST_H_

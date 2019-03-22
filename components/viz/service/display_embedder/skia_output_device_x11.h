// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_X11_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_X11_H_

#include "base/macros.h"
#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

namespace viz {

class SkiaOutputDeviceX11 : public SkiaOutputDeviceOffscreen {
 public:
  SkiaOutputDeviceX11(GrContext* gr_context, gfx::AcceleratedWidget widget);
  ~SkiaOutputDeviceX11() override;

  void Reshape(const gfx::Size& size) override;
  gfx::SwapResult SwapBuffers() override;

 private:
  XDisplay* const display_;
  const gfx::AcceleratedWidget widget_;
  GC gc_;
  XWindowAttributes attributes_;
  int bpp_;
  bool support_rendr_;
  SkBitmap sk_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceX11);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_X11_H_

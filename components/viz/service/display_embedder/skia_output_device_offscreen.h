// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_OFFSCREEN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_OFFSCREEN_H_

#include "base/macros.h"
#include "components/viz/service/display_embedder/skia_output_device.h"

class GrContext;

namespace viz {

class SkiaOutputDeviceOffscreen : public SkiaOutputDevice {
 public:
  explicit SkiaOutputDeviceOffscreen(GrContext* gr_context);
  ~SkiaOutputDeviceOffscreen() override;

  sk_sp<SkSurface> DrawSurface() override;
  void Reshape(const gfx::Size& size) override;
  gfx::SwapResult SwapBuffers() override;

 private:
  GrContext* const gr_context_;
  sk_sp<SkSurface> draw_surface_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceOffscreen);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_OFFSCREEN_H_

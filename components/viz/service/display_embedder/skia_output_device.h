// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/swap_result.h"

class SkSurface;

namespace viz {

class SkiaOutputDevice {
 public:
  SkiaOutputDevice();
  virtual ~SkiaOutputDevice();

  // SkSurface that can be drawn to.
  virtual sk_sp<SkSurface> DrawSurface() = 0;

  // Changes the size of draw surface and invalidates it's contents.
  virtual void Reshape(const gfx::Size& size) = 0;

  // Presents DrawSurface.
  virtual gfx::SwapResult SwapBuffers() = 0;

  virtual bool SupportPostSubBuffer();

  virtual gfx::SwapResult PostSubBuffer(const gfx::Rect& rect);

 private:
  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDevice);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_

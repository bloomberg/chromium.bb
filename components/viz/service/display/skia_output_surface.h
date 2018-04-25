// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_OUTPUT_SURFACE_H_

#include "components/viz/service/display/output_surface.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkCanvas;
class SkImage;

namespace viz {

struct ResourceMetadata;

// This class extends the OutputSurface for SkiaRenderer needs. In future, the
// SkiaRenderer will be the only renderer. When other renderers are removed,
// we will replace OutputSurface with SkiaOutputSurface, and remove all
// OutputSurface's methods which are not useful for SkiaRenderer.
class VIZ_SERVICE_EXPORT SkiaOutputSurface : public OutputSurface {
 public:
  explicit SkiaOutputSurface(scoped_refptr<ContextProvider> context_provider);
  ~SkiaOutputSurface() override;

  // Get a SkCanvas for the current frame. The SkiaRenderer will use this
  // SkCanvas to draw quads. This class retains the ownership of the SkCanvas,
  // And this SkCanvas may become invalid, when the frame is swapped out.
  virtual SkCanvas* GetSkCanvasForCurrentFrame() = 0;

  // Make a SkImage from the given |metadata|. The SkiaRenderer can use the
  // image with SkCanvas returned by |GetSkCanvasForCurrentFrame|, but Skia will
  // not read the content of the resource until the sync token in the |metadata|
  // is satisfied. The SwapBuffers should take care of this by scheduling a GPU
  // task with all resource sync tokens recorded by MakePromiseSkImage for the
  // current frame.
  virtual sk_sp<SkImage> MakePromiseSkImage(ResourceMetadata metadata) = 0;

  // Swaps the current backbuffer to the screen and return a sync token which
  // can be waited on in a command buffer to ensure the frame is completed. This
  // token is released when the GPU ops from drawing the frame have been seen
  // and processed by the GPU main.
  // TODO(penghuang): replace OutputSurface::SwapBuffers with this method when
  // SkiaRenderer and DDL are used everywhere.
  virtual gpu::SyncToken SkiaSwapBuffers(OutputSurfaceFrame frame) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurface);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_OUTPUT_SURFACE_H_

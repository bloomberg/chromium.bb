// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AURA_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_AURA_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "content/browser/aura/browser_compositor_output_surface.h"

namespace content {

// Adapts a WebGraphicsContext3DCommandBufferImpl into a
// cc::OutputSurface that also handles vsync parameter updates
// arriving from the GPU process.
class GpuBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  GpuBrowserCompositorOutputSurface(
      const scoped_refptr<ContextProviderCommandBuffer>& context,
      int surface_id,
      IDMap<BrowserCompositorOutputSurface>* output_surface_map,
      base::MessageLoopProxy* compositor_message_loop,
      base::WeakPtr<ui::Compositor> compositor);

  virtual ~GpuBrowserCompositorOutputSurface();

 private:
  // cc::OutputSurface implementation.
  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(GpuBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_AURA_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DELEGATING_RENDERER_H_
#define CC_DELEGATING_RENDERER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/renderer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace cc {

class OutputSurface;
class ResourceProvider;

class CC_EXPORT DelegatingRenderer :
    public Renderer,
    public NON_EXPORTED_BASE(
        WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback)
{
 public:
  static scoped_ptr<DelegatingRenderer> Create(
      RendererClient* client,
      OutputSurface* output_surface,
      ResourceProvider* resource_provider);
  virtual ~DelegatingRenderer();

  virtual const RendererCapabilities& capabilities() const OVERRIDE;

  virtual void drawFrame(RenderPassList& render_passes_in_draw_order) OVERRIDE;

  virtual void finish() OVERRIDE {}

  virtual bool swapBuffers() OVERRIDE;

  virtual void getFramebufferPixels(void *pixels,
                                    const gfx::Rect& rect) OVERRIDE;

  virtual void receiveCompositorFrameAck(const CompositorFrameAck&) OVERRIDE;

  virtual bool isContextLost() OVERRIDE;

  virtual void setVisible(bool) OVERRIDE;

  virtual void sendManagedMemoryStats(size_t bytes_visible,
                                      size_t bytes_visible_and_nearby,
                                      size_t bytes_allocated) OVERRIDE {}

  // WebGraphicsContext3D::WebGraphicsContextLostCallback implementation.
  virtual void onContextLost() OVERRIDE;

private:
  DelegatingRenderer(RendererClient* client,
                     OutputSurface* output_surface,
                     ResourceProvider* resource_provider);
  bool Initialize();

  OutputSurface* output_surface_;
  ResourceProvider* resource_provider_;
  RendererCapabilities capabilities_;
  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingRenderer);
};

}  // namespace cc

#endif  // CC_DELEGATING_RENDERER_H_

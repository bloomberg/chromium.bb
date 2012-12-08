 /*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CC_DELEGATING_RENDERER_H_
#define CC_DELEGATING_RENDERER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/renderer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace cc {

class ResourceProvider;

class CC_EXPORT DelegatingRenderer :
    public Renderer,
    public NON_EXPORTED_BASE(
        WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback)
{
 public:
  static scoped_ptr<DelegatingRenderer> Create(
      RendererClient* client, ResourceProvider* resource_provider);
  virtual ~DelegatingRenderer();

  virtual const RendererCapabilities& capabilities() const OVERRIDE;

  virtual void drawFrame(RenderPassList& render_passes_in_draw_order,
                         RenderPassIdHashMap& render_passes_by_id) OVERRIDE;

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
                     ResourceProvider* resource_provider);
  bool Initialize();

  ResourceProvider* resource_provider_;
  RendererCapabilities capabilities_;
  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingRenderer);
};

}  // namespace cc

#endif  // CC_DELEGATING_RENDERER_H_

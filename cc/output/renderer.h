// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_RENDERER_H_
#define CC_OUTPUT_RENDERER_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/managed_memory_policy.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

class CompositorFrameAck;
class CompositorFrameMetadata;
class ScopedResource;

class CC_EXPORT RendererClient {
 public:
  virtual gfx::Size DeviceViewportSize() const = 0;
  virtual const LayerTreeSettings& Settings() const = 0;
  virtual void DidLoseOutputSurface() = 0;
  virtual void OnSwapBuffersComplete() = 0;
  virtual void SetFullRootLayerDamage() = 0;
  virtual void SetManagedMemoryPolicy(const ManagedMemoryPolicy& policy) = 0;
  virtual void EnforceManagedMemoryPolicy(
      const ManagedMemoryPolicy& policy) = 0;
  virtual bool HasImplThread() const = 0;
  virtual bool ShouldClearRootRenderPass() const = 0;
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const = 0;

 protected:
  virtual ~RendererClient() {}
};

class CC_EXPORT Renderer {
 public:
  virtual ~Renderer() {}

  virtual const RendererCapabilities& Capabilities() const = 0;

  const LayerTreeSettings& Settings() const { return client_->Settings(); }

  gfx::Size ViewportSize() const { return client_->DeviceViewportSize(); }
  int ViewportWidth() const { return ViewportSize().width(); }
  int ViewportHeight() const { return ViewportSize().height(); }

  virtual void ViewportChanged() {}
  virtual void ReceiveCompositorFrameAck(const CompositorFrameAck& ack) {}

  virtual void DecideRenderPassAllocationsForFrame(
      const RenderPassList& render_passes_in_draw_order) {}
  virtual bool HaveCachedResourcesForRenderPassId(RenderPass::Id id) const;

  // This passes ownership of the render passes to the renderer. It should
  // consume them, and empty the list.
  virtual void DrawFrame(RenderPassList* render_passes_in_draw_order) = 0;

  // Waits for rendering to finish.
  virtual void Finish() = 0;

  virtual void DoNoOp() {}

  // Puts backbuffer onscreen.
  virtual bool SwapBuffers() = 0;

  virtual void GetFramebufferPixels(void* pixels, gfx::Rect rect) = 0;

  virtual bool IsContextLost();

  virtual void SetVisible(bool visible) = 0;

  virtual void SendManagedMemoryStats(size_t bytes_visible,
                                      size_t bytes_visible_and_nearby,
                                      size_t bytes_allocated) = 0;

 protected:
  explicit Renderer(RendererClient* client)
      : client_(client) {}

  RendererClient* client_;

  DISALLOW_COPY_AND_ASSIGN(Renderer);
};

}

#endif  // CC_OUTPUT_RENDERER_H_

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SOFTWARE_RENDERER_H_
#define CC_OUTPUT_SOFTWARE_RENDERER_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/direct_renderer.h"

namespace cc {

class OutputSurface;
class SoftwareOutputDevice;
class DebugBorderDrawQuad;
class RendererClient;
class RenderPassDrawQuad;
class ResourceProvider;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;

class CC_EXPORT SoftwareRenderer : public DirectRenderer {
 public:
  static scoped_ptr<SoftwareRenderer> Create(
      RendererClient* client,
      OutputSurface* output_surface,
      ResourceProvider* resource_provider);

  virtual ~SoftwareRenderer();
  virtual const RendererCapabilities& Capabilities() const OVERRIDE;
  virtual void ViewportChanged() OVERRIDE;
  virtual void Finish() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual void GetFramebufferPixels(void* pixels, gfx::Rect rect) OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual void SendManagedMemoryStats(
      size_t bytes_visible,
      size_t bytes_visible_and_nearby,
      size_t bytes_allocated) OVERRIDE  {}
  virtual void ReceiveCompositorFrameAck(
      const CompositorFrameAck& ack) OVERRIDE;

 protected:
  virtual void BindFramebufferToOutputSurface(DrawingFrame* frame) OVERRIDE;
  virtual bool BindFramebufferToTexture(
      DrawingFrame* frame,
      const ScopedResource* texture,
      gfx::Rect framebuffer_rect) OVERRIDE;
  virtual void SetDrawViewportSize(gfx::Size viewport_size) OVERRIDE;
  virtual void SetScissorTestRect(gfx::Rect scissor_rect) OVERRIDE;
  virtual void ClearFramebuffer(DrawingFrame* frame) OVERRIDE;
  virtual void DoDrawQuad(DrawingFrame* frame, const DrawQuad* quad) OVERRIDE;
  virtual void BeginDrawingFrame(DrawingFrame* frame) OVERRIDE;
  virtual void FinishDrawingFrame(DrawingFrame* frame) OVERRIDE;
  virtual bool FlippedFramebuffer() const OVERRIDE;
  virtual void EnsureScissorTestEnabled() OVERRIDE;
  virtual void EnsureScissorTestDisabled() OVERRIDE;

 private:
  SoftwareRenderer(
      RendererClient* client,
      OutputSurface* output_surface,
      ResourceProvider* resource_provider);

  void ClearCanvas(SkColor color);
  void SetClipRect(gfx::Rect rect);
  bool IsSoftwareResource(ResourceProvider::ResourceId resource_id) const;

  void DrawDebugBorderQuad(const DrawingFrame* frame,
                           const DebugBorderDrawQuad* quad);
  void DrawSolidColorQuad(const DrawingFrame* frame,
                          const SolidColorDrawQuad* quad);
  void DrawTextureQuad(const DrawingFrame* frame,
                       const TextureDrawQuad* quad);
  void DrawTileQuad(const DrawingFrame* frame,
                    const TileDrawQuad* quad);
  void DrawRenderPassQuad(const DrawingFrame* frame,
                          const RenderPassDrawQuad* quad);
  void DrawUnsupportedQuad(const DrawingFrame* frame,
                           const DrawQuad* quad);

  RendererCapabilities capabilities_;
  bool visible_;
  bool is_scissor_enabled_;
  gfx::Rect scissor_rect_;

  OutputSurface* output_surface_;
  SoftwareOutputDevice* output_device_;
  SkCanvas* root_canvas_;
  SkCanvas* current_canvas_;
  SkPaint current_paint_;
  scoped_ptr<ResourceProvider::ScopedWriteLockSoftware>
      current_framebuffer_lock_;
  CompositorFrame compositor_frame_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareRenderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_SOFTWARE_RENDERER_H_

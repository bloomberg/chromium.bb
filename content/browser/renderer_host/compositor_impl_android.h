// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/layer_tree_host_client.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/public/browser/android/compositor.h"

struct ANativeWindow;

namespace cc {
class InputHandler;
class Layer;
class LayerTreeHost;
}

namespace content {
class GraphicsContext;

// -----------------------------------------------------------------------------
// Browser-side compositor that manages a tree of content and UI layers.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT CompositorImpl
    : public Compositor,
      public cc::LayerTreeHostClient,
      public WebGraphicsContext3DSwapBuffersClient {
 public:
  explicit CompositorImpl(Compositor::Client* client);
  virtual ~CompositorImpl();

  static bool IsInitialized();
  static bool IsThreadingEnabled();

  // Returns true if initialized with DIRECT_CONTEXT_ON_DRAW_THREAD.
  static bool UsesDirectGL();

  // Compositor implementation.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> root) OVERRIDE;
  virtual void SetWindowSurface(ANativeWindow* window) OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual void setDeviceScaleFactor(float factor) OVERRIDE;
  virtual void SetWindowBounds(const gfx::Size& size) OVERRIDE;
  virtual void SetHasTransparentBackground(bool flag) OVERRIDE;
  virtual bool CompositeAndReadback(
      void *pixels, const gfx::Rect& rect) OVERRIDE;
  virtual void Composite() OVERRIDE;
  virtual WebKit::WebGLId GenerateTexture(gfx::JavaBitmap& bitmap) OVERRIDE;
  virtual WebKit::WebGLId GenerateCompressedTexture(
      gfx::Size& size, int data_size, void* data) OVERRIDE;
  virtual void DeleteTexture(WebKit::WebGLId texture_id) OVERRIDE;
  virtual bool CopyTextureToBitmap(WebKit::WebGLId texture_id,
                                   gfx::JavaBitmap& bitmap) OVERRIDE;
  virtual bool CopyTextureToBitmap(WebKit::WebGLId texture_id,
                                   const gfx::Rect& sub_rect,
                                   gfx::JavaBitmap& bitmap) OVERRIDE;

  // LayerTreeHostClient implementation.
  virtual void willBeginFrame() OVERRIDE {}
  virtual void didBeginFrame() OVERRIDE {}
  virtual void animate(double monotonicFrameBeginTime) OVERRIDE;
  virtual void layout() OVERRIDE;
  virtual void applyScrollAndScale(gfx::Vector2d scrollDelta,
                                   float pageScale) OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> createOutputSurface() OVERRIDE;
  virtual scoped_ptr<cc::InputHandler> createInputHandler() OVERRIDE;
  virtual void didRecreateOutputSurface(bool success) OVERRIDE;
  virtual void willCommit() OVERRIDE {}
  virtual void didCommit() OVERRIDE;
  virtual void didCommitAndDrawFrame() OVERRIDE;
  virtual void didCompleteSwapBuffers() OVERRIDE;
  virtual void scheduleComposite() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE;

  // WebGraphicsContext3DSwapBuffersClient implementation.
  virtual void OnViewContextSwapBuffersPosted() OVERRIDE;
  virtual void OnViewContextSwapBuffersComplete() OVERRIDE;
  virtual void OnViewContextSwapBuffersAborted() OVERRIDE;

 private:
  WebKit::WebGLId BuildBasicTexture();
  WebKit::WGC3Denum GetGLFormatForBitmap(gfx::JavaBitmap& bitmap);
  WebKit::WGC3Denum GetGLTypeForBitmap(gfx::JavaBitmap& bitmap);

  scoped_refptr<cc::Layer> root_layer_;
  scoped_ptr<cc::LayerTreeHost> host_;

  gfx::Size size_;
  bool has_transparent_background_;

  ANativeWindow* window_;
  int surface_id_;

  Compositor::Client* client_;
  base::WeakPtrFactory<CompositorImpl> weak_factory_;

  scoped_refptr<cc::ContextProvider> null_offscreen_context_provider_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImpl);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

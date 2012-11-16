// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeViewClient.h"

struct ANativeWindow;

namespace webkit {
class WebCompositorSupportImpl;
}

namespace content {
class GraphicsContext;

// -----------------------------------------------------------------------------
// Browser-side compositor that manages a tree of content and UI layers.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT CompositorImpl : public Compositor,
                                      public WebKit::WebLayerTreeViewClient {
 public:
  explicit CompositorImpl(Compositor::Client* client);
  virtual ~CompositorImpl();

  static webkit::WebCompositorSupportImpl* CompositorSupport();
  static bool IsInitialized();

  // Returns true if initialized with DIRECT_CONTEXT_ON_DRAW_THREAD.
  static bool UsesDirectGL();

  // Compositor implementation.
  virtual void SetRootLayer(WebKit::WebLayer* root) OVERRIDE;
  virtual void SetWindowSurface(ANativeWindow* window) OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual void SetWindowBounds(const gfx::Size& size) OVERRIDE;
  virtual bool CompositeAndReadback(
      void *pixels, const gfx::Rect& rect) OVERRIDE;
  virtual void Composite() OVERRIDE;
  virtual WebKit::WebGLId GenerateTexture(gfx::JavaBitmap& bitmap) OVERRIDE;
  virtual WebKit::WebGLId GenerateCompressedTexture(
      gfx::Size& size, int data_size, void* data) OVERRIDE;
  virtual void DeleteTexture(WebKit::WebGLId texture_id) OVERRIDE;
  virtual void CopyTextureToBitmap(WebKit::WebGLId texture_id,
                                   gfx::JavaBitmap& bitmap) OVERRIDE;

  // WebLayerTreeViewClient implementation.
  virtual void updateAnimations(double frameBeginTime) OVERRIDE;
  virtual void layout() OVERRIDE;
  virtual void applyScrollAndScale(const WebKit::WebSize& scrollDelta,
                                   float scaleFactor) OVERRIDE;
  virtual WebKit::WebCompositorOutputSurface* createOutputSurface() OVERRIDE;
  virtual void didRecreateOutputSurface(bool success) OVERRIDE;
  virtual void didCommit() OVERRIDE;
  virtual void didCommitAndDrawFrame() OVERRIDE;
  virtual void didCompleteSwapBuffers() OVERRIDE;
  virtual void scheduleComposite() OVERRIDE;

 private:
  WebKit::WebGLId BuildBasicTexture();
  WebKit::WGC3Denum GetGLFormatForBitmap(gfx::JavaBitmap& bitmap);
  WebKit::WGC3Denum GetGLTypeForBitmap(gfx::JavaBitmap& bitmap);

  scoped_ptr<WebKit::WebLayer> root_layer_;
  scoped_ptr<WebKit::WebLayerTreeView> host_;

  gfx::Size size_;

  ANativeWindow* window_;
  int surface_id_;

  Compositor::Client* client_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImpl);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

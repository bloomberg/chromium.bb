// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeViewClient.h"

struct ANativeWindow;

namespace content {
class GraphicsContext;

// -----------------------------------------------------------------------------
// Browser-side compositor that manages a tree of content and UI layers.
// -----------------------------------------------------------------------------
class CompositorImpl : public Compositor,
                       public WebKit::WebLayerTreeViewClient {
 public:
  CompositorImpl();
  virtual ~CompositorImpl();

  // Compositor implementation.
  virtual void SetRootLayer(WebKit::WebLayer* root) OVERRIDE;
  virtual void SetWindowSurface(ANativeWindow* window) OVERRIDE;
  virtual void SetWindowBounds(const gfx::Size& size) OVERRIDE;
  virtual void OnSurfaceUpdated(
      const SurfacePresentedCallback& callback) OVERRIDE;

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
  scoped_ptr<WebKit::WebLayer> root_layer_;
  scoped_ptr<WebKit::WebLayerTreeView> host_;

  scoped_ptr<GraphicsContext> context_;

  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImpl);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

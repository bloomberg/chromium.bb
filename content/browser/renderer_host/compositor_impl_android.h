// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/common/content_export.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/base/android/window_android_compositor.h"

class SkBitmap;
struct ANativeWindow;

namespace cc {
class InputHandlerClient;
class Layer;
class LayerTreeHost;
}

namespace content {
class CompositorClient;
class GraphicsContext;

// -----------------------------------------------------------------------------
// Browser-side compositor that manages a tree of content and UI layers.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT CompositorImpl
    : public Compositor,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient,
      public ImageTransportFactoryAndroidObserver,
      public ui::WindowAndroidCompositor {
 public:
  CompositorImpl(CompositorClient* client, gfx::NativeWindow root_window);
  virtual ~CompositorImpl();

  static bool IsInitialized();

  // Creates a surface texture and returns a surface texture id. Returns -1 on
  // failure.
  static int CreateSurfaceTexture(int child_process_id);

  // Destroy all surface textures associated with |child_process_id|.
  static void DestroyAllSurfaceTextures(int child_process_id);

  // Compositor implementation.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> root) OVERRIDE;
  virtual void SetWindowSurface(ANativeWindow* window) OVERRIDE;
  virtual void SetSurface(jobject surface) OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual void setDeviceScaleFactor(float factor) OVERRIDE;
  virtual void SetWindowBounds(const gfx::Size& size) OVERRIDE;
  virtual void SetHasTransparentBackground(bool flag) OVERRIDE;
  virtual bool CompositeAndReadback(
      void *pixels, const gfx::Rect& rect) OVERRIDE;
  virtual void Composite() OVERRIDE;
  virtual cc::UIResourceId GenerateUIResource(const SkBitmap& bitmap,
                                              bool is_transient) OVERRIDE;
  virtual cc::UIResourceId GenerateCompressedUIResource(const gfx::Size& size,
                                                        void* pixels,
                                                        bool is_transient)
      OVERRIDE;
  virtual void DeleteUIResource(cc::UIResourceId resource_id) OVERRIDE;

  // LayerTreeHostClient implementation.
  virtual void WillBeginMainFrame(int frame_id) OVERRIDE {}
  virtual void DidBeginMainFrame() OVERRIDE {}
  virtual void Animate(base::TimeTicks frame_begin_time) OVERRIDE {}
  virtual void Layout() OVERRIDE {}
  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float page_scale) OVERRIDE {}
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE;
  virtual void DidInitializeOutputSurface(bool success) OVERRIDE {}
  virtual void WillCommit() OVERRIDE {}
  virtual void DidCommit() OVERRIDE;
  virtual void DidCommitAndDrawFrame() OVERRIDE {}
  virtual void DidCompleteSwapBuffers() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProvider() OVERRIDE;

  // LayerTreeHostSingleThreadClient implementation.
  virtual void ScheduleComposite() OVERRIDE;
  virtual void ScheduleAnimation() OVERRIDE;
  virtual void DidPostSwapBuffers() OVERRIDE;
  virtual void DidAbortSwapBuffers() OVERRIDE;

  // ImageTransportFactoryAndroidObserver implementation.
  virtual void OnLostResources() OVERRIDE;

  // WindowAndroidCompositor implementation.
  virtual void AttachLayerForReadback(scoped_refptr<cc::Layer> layer) OVERRIDE;

 private:
  cc::UIResourceId GenerateUIResourceFromUIResourceBitmap(
      const cc::UIResourceBitmap& bitmap,
      bool is_transient);

  scoped_refptr<cc::Layer> root_layer_;
  scoped_ptr<cc::LayerTreeHost> host_;

  gfx::Size size_;
  bool has_transparent_background_;
  float device_scale_factor_;

  ANativeWindow* window_;
  int surface_id_;

  CompositorClient* client_;

  scoped_refptr<cc::ContextProvider> null_offscreen_context_provider_;

  typedef base::ScopedPtrHashMap<cc::UIResourceId, cc::UIResourceClient>
      UIResourceMap;
  UIResourceMap ui_resource_map_;

  gfx::NativeWindow root_window_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImpl);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

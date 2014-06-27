// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

#include "android_webview/browser/shared_renderer_state.h"
#include "base/memory/scoped_ptr.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"

struct AwDrawGLInfo;

namespace cc {
class DelegatedFrameProvider;
class DelegatedRendererLayer;
class Layer;
class LayerTreeHost;
}

namespace android_webview {

class AwGLSurface;
class ParentOutputSurface;

class HardwareRenderer : public cc::LayerTreeHostClient,
                         public cc::LayerTreeHostSingleThreadClient,
                         public cc::DelegatedFrameResourceCollectionClient {
 public:
  explicit HardwareRenderer(SharedRendererState* state);
  virtual ~HardwareRenderer();

  void DrawGL(bool stencil_enabled,
              int framebuffer_binding_ext,
              AwDrawGLInfo* draw_info);

  // cc::LayerTreeHostClient overrides.
  virtual void WillBeginMainFrame(int frame_id) OVERRIDE {}
  virtual void DidBeginMainFrame() OVERRIDE;
  virtual void Animate(base::TimeTicks frame_begin_time) OVERRIDE {}
  virtual void Layout() OVERRIDE {}
  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float page_scale) OVERRIDE {}
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      bool fallback) OVERRIDE;
  virtual void DidInitializeOutputSurface() OVERRIDE {}
  virtual void WillCommit() OVERRIDE {}
  virtual void DidCommit() OVERRIDE {}
  virtual void DidCommitAndDrawFrame() OVERRIDE {}
  virtual void DidCompleteSwapBuffers() OVERRIDE {}

  // cc::LayerTreeHostSingleThreadClient overrides.
  virtual void ScheduleComposite() OVERRIDE {}
  virtual void ScheduleAnimation() OVERRIDE {}
  virtual void DidPostSwapBuffers() OVERRIDE {}
  virtual void DidAbortSwapBuffers() OVERRIDE {}

  // cc::DelegatedFrameResourceCollectionClient overrides.
  virtual void UnusedResourcesAreAvailable() OVERRIDE;

 private:
  SharedRendererState* shared_renderer_state_;

  typedef void* EGLContext;
  EGLContext last_egl_context_;

  // Information about last delegated frame.
  gfx::Size frame_size_;
  gfx::Vector2d scroll_offset_;

  // Information from draw.
  gfx::Size viewport_;
  gfx::Rect clip_;
  bool stencil_enabled_;
  bool viewport_clip_valid_for_dcheck_;

  scoped_refptr<AwGLSurface> gl_surface_;

  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;
  scoped_refptr<cc::Layer> root_layer_;

  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection_;
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider_;
  scoped_refptr<cc::DelegatedRendererLayer> delegated_layer_;

  // This is owned indirectly by |layer_tree_host_|.
  ParentOutputSurface* output_surface_;

  DISALLOW_COPY_AND_ASSIGN(HardwareRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

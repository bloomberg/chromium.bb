// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

#include <memory>

#include "android_webview/browser/render_thread_manager.h"
#include "base/macros.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"

struct AwDrawGLInfo;

namespace cc {
class Display;
class SurfaceFactory;
class SurfaceIdAllocator;
class SurfaceManager;
}

namespace android_webview {

class AwGLSurface;
class ChildFrame;
class ParentOutputSurface;
class ScopedAppGLStateRestore;

class HardwareRenderer : public cc::DisplayClient,
                         public cc::SurfaceFactoryClient {
 public:
  explicit HardwareRenderer(RenderThreadManager* state);
  ~HardwareRenderer() override;

  void DrawGL(AwDrawGLInfo* draw_info, const ScopedAppGLStateRestore& gl_state);
  void CommitFrame();

  void SetBackingFrameBufferObject(int framebuffer_binding_ext);

 private:
  // cc::DisplayClient overrides.
  void OutputSurfaceLost() override {}
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  void ReturnResourcesInChildFrame();
  void ReturnResourcesToCompositor(const cc::ReturnedResourceArray& resources,
                                   uint32_t compositor_routing_id,
                                   uint32_t output_surface_id);

  RenderThreadManager* render_thread_manager_;

  typedef void* EGLContext;
  EGLContext last_egl_context_;

  // Information about last delegated frame.
  gfx::Size frame_size_;

  // Infromation from UI on last commit.
  gfx::Vector2d scroll_offset_;

  // This holds the last ChildFrame received. Contains the frame info of the
  // last frame. The |frame| member may be null if it's already submitted to
  // SurfaceFactory.
  std::unique_ptr<ChildFrame> child_frame_;

  scoped_refptr<AwGLSurface> gl_surface_;

  std::unique_ptr<cc::SurfaceManager> surface_manager_;
  std::unique_ptr<cc::Display> display_;
  std::unique_ptr<cc::SurfaceFactory> surface_factory_;
  std::unique_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  cc::SurfaceId child_id_;
  cc::SurfaceId root_id_;
  uint32_t compositor_id_;
  // HardwareRenderer guarantees resources are returned in the order of
  // output_surface_id, and resources for old output surfaces are dropped.
  uint32_t last_committed_output_surface_id_;
  uint32_t last_submitted_output_surface_id_;

  // This is owned by |display_|.
  ParentOutputSurface* output_surface_;

  DISALLOW_COPY_AND_ASSIGN(HardwareRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

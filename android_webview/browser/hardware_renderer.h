// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

#include <memory>

#include "android_webview/browser/compositor_id.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
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

class ChildFrame;
class RenderThreadManager;
class ScopedAppGLStateRestore;
class SurfacesInstance;

class HardwareRenderer : public cc::SurfaceFactoryClient {
 public:
  explicit HardwareRenderer(RenderThreadManager* state);
  ~HardwareRenderer() override;

  void DrawGL(AwDrawGLInfo* draw_info, const ScopedAppGLStateRestore& gl_state);
  void CommitFrame();

  void SetBackingFrameBufferObject(int framebuffer_binding_ext);

 private:
  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  void ReturnResourcesInChildFrame();
  void ReturnResourcesToCompositor(const cc::ReturnedResourceArray& resources,
                                   const CompositorID& compositor_id,
                                   uint32_t output_surface_id);

  void AllocateSurface();
  void DestroySurface();

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

  const scoped_refptr<SurfacesInstance> surfaces_;
  const std::unique_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  std::unique_ptr<cc::SurfaceFactory> surface_factory_;
  cc::SurfaceId child_id_;
  CompositorID compositor_id_;
  // HardwareRenderer guarantees resources are returned in the order of
  // output_surface_id, and resources for old output surfaces are dropped.
  uint32_t last_committed_output_surface_id_;
  uint32_t last_submitted_output_surface_id_;

  DISALLOW_COPY_AND_ASSIGN(HardwareRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

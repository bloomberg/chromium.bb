// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

#include "android_webview/browser/shared_renderer_state.h"
#include "base/memory/scoped_ptr.h"
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

class HardwareRenderer : public cc::DisplayClient,
                         public cc::SurfaceFactoryClient {
 public:
  explicit HardwareRenderer(SharedRendererState* state);
  ~HardwareRenderer() override;

  void DrawGL(bool stencil_enabled,
              AwDrawGLInfo* draw_info);
  void CommitFrame();

  void SetBackingFrameBufferObject(int framebuffer_binding_ext);

 private:
  // cc::DisplayClient overrides.
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override {}
  void OutputSurfaceLost() override {}
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

  void ReturnResourcesInChildFrame();

  SharedRendererState* shared_renderer_state_;

  typedef void* EGLContext;
  EGLContext last_egl_context_;

  // Information about last delegated frame.
  gfx::Size frame_size_;

  // Infromation from UI on last commit.
  gfx::Vector2d scroll_offset_;

  // This holds the last ChildFrame received. Contains the frame info of the
  // last frame. The |frame| member may be null if it's already submitted to
  // SurfaceFactory.
  scoped_ptr<ChildFrame> child_frame_;

  scoped_refptr<AwGLSurface> gl_surface_;

  scoped_ptr<cc::SurfaceManager> surface_manager_;
  scoped_ptr<cc::Display> display_;
  scoped_ptr<cc::SurfaceFactory> surface_factory_;
  scoped_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  cc::SurfaceId child_id_;
  cc::SurfaceId root_id_;

  // This is owned by |display_|.
  ParentOutputSurface* output_surface_;

  DISALLOW_COPY_AND_ASSIGN(HardwareRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

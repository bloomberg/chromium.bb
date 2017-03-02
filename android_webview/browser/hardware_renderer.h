// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

#include <memory>

#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/compositor_id.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"

struct AwDrawGLInfo;

namespace cc {
class CompositorFrameSinkSupport;
class LocalSurfaceIdAllocator;
}

namespace android_webview {

class ChildFrame;
class RenderThreadManager;
class SurfacesInstance;

class HardwareRenderer : public cc::CompositorFrameSinkSupportClient {
 public:
  // Two rules:
  // 1) Never wait on |new_frame| on the UI thread, or in kModeSync. Otherwise
  //    this defeats the purpose of having a future.
  // 2) Never replace a non-empty frames with an empty frame.
  // The only way to do both is to hold up to two frames here. This is a helper
  // method to do this. General pattern is call this method to prune existing
  // queue, and then append the new frame. Wait on all frames in queue. Then
  // remove all except the latest non-empty frame. If all frames are empty,
  // then the deque is cleared. Return any non-empty frames that are pruned.
  // Return value does not guarantee relative order is maintained.
  static ChildFrameQueue WaitAndPruneFrameQueue(ChildFrameQueue* child_frames);

  explicit HardwareRenderer(RenderThreadManager* state);
  ~HardwareRenderer() override;

  void DrawGL(AwDrawGLInfo* draw_info);
  void CommitFrame();

 private:
  // cc::CompositorFrameSinkSupportClient implementation.
  void DidReceiveCompositorFrameAck() override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface(const cc::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;

  void ReturnChildFrame(std::unique_ptr<ChildFrame> child_frame);
  void ReturnResourcesToCompositor(const cc::ReturnedResourceArray& resources,
                                   const CompositorID& compositor_id,
                                   uint32_t compositor_frame_sink_id);

  void AllocateSurface();
  void DestroySurface();

  void CreateNewCompositorFrameSinkSupport();

  RenderThreadManager* render_thread_manager_;

  typedef void* EGLContext;
  EGLContext last_egl_context_;

  // Information about last delegated frame.
  gfx::Size frame_size_;

  // Infromation from UI on last commit.
  gfx::Vector2d scroll_offset_;

  ChildFrameQueue child_frame_queue_;

  // This holds the last ChildFrame received. Contains the frame info of the
  // last frame. The |frame| member is always null since frame has already
  // been submitted.
  std::unique_ptr<ChildFrame> child_frame_;

  const scoped_refptr<SurfacesInstance> surfaces_;
  cc::FrameSinkId frame_sink_id_;
  const std::unique_ptr<cc::LocalSurfaceIdAllocator>
      local_surface_id_allocator_;
  std::unique_ptr<cc::CompositorFrameSinkSupport> support_;
  cc::LocalSurfaceId child_id_;
  CompositorID compositor_id_;
  // HardwareRenderer guarantees resources are returned in the order of
  // compositor_frame_sink_id, and resources for old output surfaces are
  // dropped.
  uint32_t last_committed_compositor_frame_sink_id_;
  uint32_t last_submitted_compositor_frame_sink_id_;

  DISALLOW_COPY_AND_ASSIGN(HardwareRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

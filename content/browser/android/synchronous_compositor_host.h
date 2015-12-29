// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_
#define CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "cc/output/compositor_frame.h"
#include "content/browser/android/synchronous_compositor_base.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_f.h"

namespace IPC {
class Sender;
}

namespace content {

class RenderWidgetHostViewAndroid;
class SynchronousCompositorClient;
struct DidOverscrollParams;
struct SyncCompositorCommonBrowserParams;
struct SyncCompositorCommonRendererParams;

class SynchronousCompositorHost : public SynchronousCompositorBase {
 public:
  ~SynchronousCompositorHost() override;

  // SynchronousCompositor overrides.
  scoped_ptr<cc::CompositorFrame> DemandDrawHw(
      const gfx::Size& surface_size,
      const gfx::Transform& transform,
      const gfx::Rect& viewport,
      const gfx::Rect& clip,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override;
  bool DemandDrawSw(SkCanvas* canvas) override;
  void ReturnResources(const cc::CompositorFrameAck& frame_ack) override;
  void SetMemoryPolicy(size_t bytes_limit) override;
  void DidChangeRootLayerScrollOffset(
      const gfx::ScrollOffset& root_offset) override;
  void SetIsActive(bool is_active) override;
  void OnComputeScroll(base::TimeTicks animation_time) override;

  // SynchronousCompositorBase overrides.
  InputEventAckState HandleInputEvent(
      const blink::WebInputEvent& input_event) override;
  void BeginFrame(const cc::BeginFrameArgs& args) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidBecomeCurrent() override;

 private:
  class ScopedSendZeroMemory;
  struct SharedMemoryWithSize;
  friend class ScopedSetZeroMemory;
  friend class SynchronousCompositorBase;

  SynchronousCompositorHost(RenderWidgetHostViewAndroid* rwhva,
                            SynchronousCompositorClient* client);
  void PopulateCommonParams(SyncCompositorCommonBrowserParams* params);
  void ProcessCommonParams(const SyncCompositorCommonRendererParams& params);
  void UpdateNeedsBeginFrames();
  void UpdateFrameMetaData(const cc::CompositorFrameMetadata& frame_metadata);
  void OnOverScroll(const SyncCompositorCommonRendererParams& params,
                    const DidOverscrollParams& over_scroll_params);
  void SendAsyncCompositorStateIfNeeded();
  void UpdateStateTask();
  void SetSoftwareDrawSharedMemoryIfNeeded(size_t stride, size_t buffer_size);
  void SendZeroMemory();

  RenderWidgetHostViewAndroid* const rwhva_;
  SynchronousCompositorClient* const client_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  const int routing_id_;
  IPC::Sender* const sender_;

  bool is_active_;
  size_t bytes_limit_;
  cc::ReturnedResourceArray returned_resources_;
  scoped_ptr<SharedMemoryWithSize> software_draw_shm_;

  // Updated by both renderer and browser.
  gfx::ScrollOffset root_scroll_offset_;
  bool root_scroll_offset_updated_by_browser_;

  // From renderer.
  uint32_t renderer_param_version_;
  bool need_animate_scroll_;
  bool need_invalidate_;
  bool need_begin_frame_;
  bool did_activate_pending_tree_;

  base::WeakPtrFactory<SynchronousCompositorHost> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_

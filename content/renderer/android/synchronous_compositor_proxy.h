// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_PROXY_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_PROXY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/renderer/android/synchronous_compositor_external_begin_frame_source.h"
#include "content/renderer/android/synchronous_compositor_output_surface.h"
#include "content/renderer/input/input_handler_manager_client.h"
#include "ui/events/blink/synchronous_input_handler_proxy.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_f.h"

class SkCanvas;

namespace IPC {
class Message;
class Sender;
}  // namespace IPC

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace cc {
class CompositorFrame;
}  // namespace cc

namespace content {

class SynchronousCompositorOutputSurface;
struct SyncCompositorCommonBrowserParams;
struct SyncCompositorCommonRendererParams;
struct SyncCompositorDemandDrawHwParams;
struct SyncCompositorDemandDrawSwParams;
struct SyncCompositorSetSharedMemoryParams;

class SynchronousCompositorProxy
    : public ui::SynchronousInputHandler,
      public SynchronousCompositorExternalBeginFrameSourceClient,
      public SynchronousCompositorOutputSurfaceClient {
 public:
  SynchronousCompositorProxy(
      int routing_id,
      IPC::Sender* sender,
      SynchronousCompositorExternalBeginFrameSource* begin_frame_source,
      ui::SynchronousInputHandlerProxy* input_handler_proxy,
      InputHandlerManagerClient::Handler* handler);
  ~SynchronousCompositorProxy() override;

  // ui::SynchronousInputHandler overrides.
  void SetNeedsSynchronousAnimateInput() override;
  void UpdateRootLayerState(const gfx::ScrollOffset& total_scroll_offset,
                            const gfx::ScrollOffset& max_scroll_offset,
                            const gfx::SizeF& scrollable_size,
                            float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor) override;

  // SynchronousCompositorExternalBeginFrameSourceClient overrides.
  void OnNeedsBeginFramesChange(bool needs_begin_frames) override;

  // SynchronousCompositorOutputSurfaceClient overrides.
  void Invalidate() override;
  void SwapBuffers(uint32_t output_surface_id,
                   cc::CompositorFrame* frame) override;

  void SetOutputSurface(SynchronousCompositorOutputSurface* output_surface);
  void OnMessageReceived(const IPC::Message& message);
  bool Send(IPC::Message* message);
  void DidOverscroll(const DidOverscrollParams& did_overscroll_params);

 private:
  struct SharedMemoryWithSize;

  void ProcessCommonParams(
      const SyncCompositorCommonBrowserParams& common_params);
  void PopulateCommonParams(SyncCompositorCommonRendererParams* params) const;

  // IPC handlers.
  void HandleInputEvent(
      const SyncCompositorCommonBrowserParams& common_params,
      const blink::WebInputEvent* event,
      SyncCompositorCommonRendererParams* common_renderer_params,
      InputEventAckState* ack);
  void BeginFrame(const SyncCompositorCommonBrowserParams& common_params,
                  const cc::BeginFrameArgs& args,
                  SyncCompositorCommonRendererParams* common_renderer_params);
  void OnComputeScroll(const SyncCompositorCommonBrowserParams& common_params,
                       base::TimeTicks animation_time);
  void DemandDrawHw(const SyncCompositorCommonBrowserParams& common_params,
                    const SyncCompositorDemandDrawHwParams& params,
                    IPC::Message* reply_message);
  void SetSharedMemory(
      const SyncCompositorCommonBrowserParams& common_params,
      const SyncCompositorSetSharedMemoryParams& params,
      bool* success,
      SyncCompositorCommonRendererParams* common_renderer_params);
  void ZeroSharedMemory();
  void DemandDrawSw(const SyncCompositorCommonBrowserParams& common_params,
                    const SyncCompositorDemandDrawSwParams& params,
                    IPC::Message* reply_message);
  void SynchronouslyZoomBy(
      const SyncCompositorCommonBrowserParams& common_params,
      float zoom_delta,
      const gfx::Point& anchor,
      SyncCompositorCommonRendererParams* common_renderer_params);

  void SwapBuffersHw(uint32_t output_surface_id, cc::CompositorFrame* frame);
  void SendDemandDrawHwReply(cc::CompositorFrame* frame,
                             uint32_t output_surface_id,
                             IPC::Message* reply_message);
  void DoDemandDrawSw(const SyncCompositorDemandDrawSwParams& params);
  void SwapBuffersSw(cc::CompositorFrame* frame);
  void SendDemandDrawSwReply(bool success,
                             cc::CompositorFrame* frame,
                             IPC::Message* reply_message);
  void DidActivatePendingTree();
  void DeliverMessages();
  void SendAsyncRendererStateIfNeeded();

  const int routing_id_;
  IPC::Sender* const sender_;
  SynchronousCompositorExternalBeginFrameSource* const begin_frame_source_;
  ui::SynchronousInputHandlerProxy* const input_handler_proxy_;
  InputHandlerManagerClient::Handler* const input_handler_;
  const bool use_in_process_zero_copy_software_draw_;
  SynchronousCompositorOutputSurface* output_surface_;
  bool inside_receive_;
  IPC::Message* hardware_draw_reply_;
  IPC::Message* software_draw_reply_;

  // From browser.
  size_t bytes_limit_;
  std::unique_ptr<SharedMemoryWithSize> software_draw_shm_;

  // To browser.
  mutable uint32_t version_;  // Mustable so PopulateCommonParams can be const.
  gfx::ScrollOffset total_scroll_offset_;  // Modified by both.
  gfx::ScrollOffset max_scroll_offset_;
  gfx::SizeF scrollable_size_;
  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
  bool need_animate_scroll_;
  uint32_t need_invalidate_count_;
  bool need_begin_frame_;
  uint32_t did_activate_pending_tree_count_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_PROXY_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_host.h"

#include "base/containers/hash_tables.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "ipc/ipc_sender.h"

namespace content {

SynchronousCompositorHost::SynchronousCompositorHost(
    RenderWidgetHostViewAndroid* rwhva,
    SynchronousCompositorClient* client)
    : rwhva_(rwhva),
      client_(client),
      ui_task_runner_(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)),
      routing_id_(rwhva_->GetRenderWidgetHost()->GetRoutingID()),
      sender_(rwhva_->GetRenderWidgetHost()),
      is_active_(false),
      bytes_limit_(0u),
      renderer_param_version_(0u),
      need_animate_scroll_(false),
      need_invalidate_(false),
      need_begin_frame_(false),
      did_activate_pending_tree_(false),
      weak_ptr_factory_(this) {
  client_->DidInitializeCompositor(this);
}

SynchronousCompositorHost::~SynchronousCompositorHost() {
  client_->DidDestroyCompositor(this);
}

bool SynchronousCompositorHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorHost, message)
    IPC_MESSAGE_HANDLER(SyncCompositorHostMsg_UpdateState, ProcessCommonParams)
    IPC_MESSAGE_HANDLER(SyncCompositorHostMsg_OverScroll, OnOverScroll)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

scoped_ptr<cc::CompositorFrame> SynchronousCompositorHost::DemandDrawHw(
    const gfx::Size& surface_size,
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  SyncCompositorDemandDrawHwParams params(surface_size, transform, viewport,
                                          clip, viewport_rect_for_tile_priority,
                                          transform_for_tile_priority);
  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(new SyncCompositorMsg_DemandDrawHw(
          routing_id_, common_browser_params, params, &common_renderer_params,
          frame.get()))) {
    return nullptr;
  }
  ProcessCommonParams(common_renderer_params);
  if (!frame->delegated_frame_data) {
    // This can happen if compositor did not swap in this draw.
    frame.reset();
  }
  if (frame)
    UpdateFrameMetaData(frame->metadata);
  return frame;
}

void SynchronousCompositorHost::UpdateFrameMetaData(
    const cc::CompositorFrameMetadata& frame_metadata) {
  rwhva_->SynchronousFrameMetadata(frame_metadata);
}

bool SynchronousCompositorHost::DemandDrawSw(SkCanvas* canvas) {
  // TODO(boliu): Implement.
  return false;
}

void SynchronousCompositorHost::ReturnResources(
    const cc::CompositorFrameAck& frame_ack) {
  returned_resources_.insert(returned_resources_.end(),
                             frame_ack.resources.begin(),
                             frame_ack.resources.end());
}

void SynchronousCompositorHost::SetMemoryPolicy(size_t bytes_limit) {
  if (bytes_limit_ == bytes_limit)
    return;
  bytes_limit_ = bytes_limit;
  SendAsyncCompositorStateIfNeeded();
}

void SynchronousCompositorHost::DidChangeRootLayerScrollOffset(
    const gfx::ScrollOffset& root_offset) {
  if (root_scroll_offset_ == root_offset)
    return;
  root_scroll_offset_ = root_offset;
  SendAsyncCompositorStateIfNeeded();
}

void SynchronousCompositorHost::SendAsyncCompositorStateIfNeeded() {
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SynchronousCompositorHost::UpdateStateTask,
                            weak_ptr_factory_.GetWeakPtr()));
}

void SynchronousCompositorHost::UpdateStateTask() {
  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  sender_->Send(
      new SyncCompositorMsg_UpdateState(routing_id_, common_browser_params));
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
}

void SynchronousCompositorHost::SetIsActive(bool is_active) {
  is_active_ = is_active;
  UpdateNeedsBeginFrames();
}

void SynchronousCompositorHost::OnComputeScroll(
    base::TimeTicks animation_time) {
  if (!need_animate_scroll_)
    return;
  need_animate_scroll_ = false;

  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(new SyncCompositorMsg_ComputeScroll(
          routing_id_, common_browser_params, animation_time,
          &common_renderer_params))) {
    return;
  }
  ProcessCommonParams(common_renderer_params);
}

InputEventAckState SynchronousCompositorHost::HandleInputEvent(
    const blink::WebInputEvent& input_event) {
  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  InputEventAckState ack = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  if (!sender_->Send(new SyncCompositorMsg_HandleInputEvent(
          routing_id_, common_browser_params, &input_event,
          &common_renderer_params, &ack))) {
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  }
  ProcessCommonParams(common_renderer_params);
  return ack;
}

void SynchronousCompositorHost::BeginFrame(const cc::BeginFrameArgs& args) {
  if (!is_active_ || !need_begin_frame_)
    return;

  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(
          new SyncCompositorMsg_BeginFrame(routing_id_, common_browser_params,
                                           args, &common_renderer_params))) {
    return;
  }
  ProcessCommonParams(common_renderer_params);
}

void SynchronousCompositorHost::OnOverScroll(
    const SyncCompositorCommonRendererParams& params,
    const DidOverscrollParams& over_scroll_params) {
  ProcessCommonParams(params);
  client_->DidOverscroll(over_scroll_params.accumulated_overscroll,
                         over_scroll_params.latest_overscroll_delta,
                         over_scroll_params.current_fling_velocity);
}

void SynchronousCompositorHost::PopulateCommonParams(
    SyncCompositorCommonBrowserParams* params) {
  DCHECK(params);
  DCHECK(params->ack.resources.empty());
  params->bytes_limit = bytes_limit_;
  params->root_scroll_offset = root_scroll_offset_;
  params->ack.resources.swap(returned_resources_);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SynchronousCompositorHost::ProcessCommonParams(
    const SyncCompositorCommonRendererParams& params) {
  // Ignore if |renderer_param_version_| is newer than |params.version|. This
  // comparison takes into account when the unsigned int wraps.
  if ((renderer_param_version_ - params.version) < 0x80000000) {
    return;
  }
  renderer_param_version_ = params.version;
  need_animate_scroll_ = params.need_animate_scroll;
  if (need_begin_frame_ != params.need_begin_frame) {
    need_begin_frame_ = params.need_begin_frame;
    UpdateNeedsBeginFrames();
  }
  need_invalidate_ = need_invalidate_ || params.need_invalidate;
  did_activate_pending_tree_ =
      did_activate_pending_tree_ || params.did_activate_pending_tree;
  root_scroll_offset_ = params.total_scroll_offset;

  if (need_invalidate_) {
    need_invalidate_ = false;
    client_->PostInvalidate();
  }

  if (did_activate_pending_tree_) {
    did_activate_pending_tree_ = false;
    client_->DidUpdateContent();
  }

  // Ensure only valid values from compositor are sent to client.
  // Compositor has page_scale_factor set to 0 before initialization, so check
  // for that case here.
  if (params.page_scale_factor) {
    client_->UpdateRootLayerState(
        gfx::ScrollOffsetToVector2dF(params.total_scroll_offset),
        gfx::ScrollOffsetToVector2dF(params.max_scroll_offset),
        params.scrollable_size, params.page_scale_factor,
        params.min_page_scale_factor, params.max_page_scale_factor);
  }
}

void SynchronousCompositorHost::UpdateNeedsBeginFrames() {
  rwhva_->OnSetNeedsBeginFrames(is_active_ && need_begin_frame_);
}

}  // namespace content

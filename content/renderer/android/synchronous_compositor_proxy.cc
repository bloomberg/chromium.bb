// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_proxy.h"

#include "base/auto_reset.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/common/cc_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ui/events/latency_info.h"

namespace content {

SynchronousCompositorProxy::SynchronousCompositorProxy(
    int routing_id,
    IPC::Sender* sender,
    SynchronousCompositorOutputSurface* output_surface,
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source,
    SynchronousInputHandlerProxy* input_handler_proxy,
    InputHandlerManagerClient::Handler* handler)
    : routing_id_(routing_id),
      sender_(sender),
      output_surface_(output_surface),
      begin_frame_source_(begin_frame_source),
      input_handler_proxy_(input_handler_proxy),
      input_handler_(handler),
      inside_receive_(false),
      bytes_limit_(0u),
      version_(0u),
      page_scale_factor_(0.f),
      min_page_scale_factor_(0.f),
      max_page_scale_factor_(0.f),
      need_animate_scroll_(false),
      need_invalidate_(false),
      need_begin_frame_(false),
      did_activate_pending_tree_(false) {
  DCHECK(output_surface_);
  DCHECK(begin_frame_source_);
  DCHECK(input_handler_proxy_);
  DCHECK(input_handler_);
  output_surface_->SetSyncClient(this);
  output_surface_->SetTreeActivationCallback(
      base::Bind(&SynchronousCompositorProxy::DidActivatePendingTree,
                 base::Unretained(this)));
  begin_frame_source_->SetClient(this);
  input_handler_proxy_->SetOnlySynchronouslyAnimateRootFlings(this);
}

SynchronousCompositorProxy::~SynchronousCompositorProxy() {
  output_surface_->SetSyncClient(nullptr);
  output_surface_->SetTreeActivationCallback(base::Closure());
  begin_frame_source_->SetClient(nullptr);
  input_handler_proxy_->SetOnlySynchronouslyAnimateRootFlings(nullptr);
}

void SynchronousCompositorProxy::SetNeedsSynchronousAnimateInput() {
  need_animate_scroll_ = true;
  Invalidate();
}

void SynchronousCompositorProxy::UpdateRootLayerState(
    const gfx::ScrollOffset& total_scroll_offset,
    const gfx::ScrollOffset& max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  if (total_scroll_offset_ != total_scroll_offset ||
      max_scroll_offset_ != max_scroll_offset ||
      scrollable_size_ != scrollable_size ||
      page_scale_factor_ != page_scale_factor ||
      min_page_scale_factor_ != min_page_scale_factor ||
      max_page_scale_factor_ != max_page_scale_factor) {
    total_scroll_offset_ = total_scroll_offset;
    max_scroll_offset_ = max_scroll_offset;
    scrollable_size_ = scrollable_size;
    page_scale_factor_ = page_scale_factor;
    min_page_scale_factor_ = min_page_scale_factor;
    max_page_scale_factor_ = max_page_scale_factor;

    SendAsyncRendererStateIfNeeded();
  }
}

void SynchronousCompositorProxy::OnNeedsBeginFramesChange(
    bool needs_begin_frames) {
  if (need_begin_frame_ == needs_begin_frames)
    return;
  need_begin_frame_ = needs_begin_frames;
  SendAsyncRendererStateIfNeeded();
}

void SynchronousCompositorProxy::Invalidate() {
  need_invalidate_ = true;
  SendAsyncRendererStateIfNeeded();
}

void SynchronousCompositorProxy::DidActivatePendingTree() {
  did_activate_pending_tree_ = true;
  SendAsyncRendererStateIfNeeded();
  DeliverMessages();
}

void SynchronousCompositorProxy::DeliverMessages() {
  ScopedVector<IPC::Message> messages;
  output_surface_->GetMessagesToDeliver(&messages);
  for (auto* message : messages) {
    Send(message);
  }
  messages.weak_clear();  // Don't double delete.
}

void SynchronousCompositorProxy::SendAsyncRendererStateIfNeeded() {
  if (inside_receive_)
    return;
  SyncCompositorCommonRendererParams params;
  PopulateCommonParams(&params);
  Send(new SyncCompositorHostMsg_UpdateState(routing_id_, params));
}

void SynchronousCompositorProxy::PopulateCommonParams(
    SyncCompositorCommonRendererParams* params) {
  params->version = ++version_;
  params->total_scroll_offset = total_scroll_offset_;
  params->max_scroll_offset = max_scroll_offset_;
  params->scrollable_size = scrollable_size_;
  params->page_scale_factor = page_scale_factor_;
  params->min_page_scale_factor = min_page_scale_factor_;
  params->max_page_scale_factor = max_page_scale_factor_;
  params->need_animate_scroll = need_animate_scroll_;
  params->need_invalidate = need_invalidate_;
  params->need_begin_frame = need_begin_frame_;
  params->did_activate_pending_tree = did_activate_pending_tree_;

  need_invalidate_ = false;
  did_activate_pending_tree_ = false;
}

void SynchronousCompositorProxy::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(!inside_receive_);
  base::AutoReset<bool> scoped_inside_receive(&inside_receive_, true);
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorProxy, message)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_HandleInputEvent, HandleInputEvent)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_BeginFrame, BeginFrame)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ComputeScroll, OnComputeScroll)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_DemandDrawHw, DemandDrawHw)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_UpdateState, ProcessCommonParams)
  IPC_END_MESSAGE_MAP()
}

bool SynchronousCompositorProxy::Send(IPC::Message* message) {
  return sender_->Send(message);
}

void SynchronousCompositorProxy::HandleInputEvent(
    const SyncCompositorCommonBrowserParams& common_params,
    const blink::WebInputEvent* event,
    SyncCompositorCommonRendererParams* common_renderer_params,
    InputEventAckState* ack) {
  ProcessCommonParams(common_params);
  DCHECK(!input_handler_->is_null());
  ui::LatencyInfo latency;
  *ack = input_handler_->Run(routing_id_, event, &latency);
  PopulateCommonParams(common_renderer_params);
}

void SynchronousCompositorProxy::BeginFrame(
    const SyncCompositorCommonBrowserParams& common_params,
    const cc::BeginFrameArgs& args,
    SyncCompositorCommonRendererParams* common_renderer_params) {
  ProcessCommonParams(common_params);
  if (need_begin_frame_) {
    begin_frame_source_->BeginFrame(args);
  }
  PopulateCommonParams(common_renderer_params);
}

void SynchronousCompositorProxy::DemandDrawHw(
    const SyncCompositorCommonBrowserParams& common_params,
    const SyncCompositorDemandDrawHwParams& params,
    SyncCompositorCommonRendererParams* common_renderer_params,
    cc::CompositorFrame* frame) {
  DCHECK(frame);
  ProcessCommonParams(common_params);
  scoped_ptr<cc::CompositorFrame> frame_ptr = output_surface_->DemandDrawHw(
      params.surface_size, params.transform, params.viewport, params.clip,
      params.viewport_rect_for_tile_priority,
      params.transform_for_tile_priority);
  if (frame_ptr) {
    frame_ptr->AssignTo(frame);
    DeliverMessages();
  }
  PopulateCommonParams(common_renderer_params);
}

void SynchronousCompositorProxy::OnComputeScroll(
    const SyncCompositorCommonBrowserParams& common_params,
    base::TimeTicks animation_time,
    SyncCompositorCommonRendererParams* common_renderer_params) {
  ProcessCommonParams(common_params);
  if (need_animate_scroll_) {
    need_animate_scroll_ = false;
    input_handler_proxy_->SynchronouslyAnimate(animation_time);
  }
  PopulateCommonParams(common_renderer_params);
}

void SynchronousCompositorProxy::DidOverscroll(
    const DidOverscrollParams& did_overscroll_params) {
  SyncCompositorCommonRendererParams params;
  PopulateCommonParams(&params);
  Send(new SyncCompositorHostMsg_OverScroll(routing_id_, params,
                                            did_overscroll_params));
}

void SynchronousCompositorProxy::ProcessCommonParams(
    const SyncCompositorCommonBrowserParams& common_params) {
  if (bytes_limit_ != common_params.bytes_limit) {
    bytes_limit_ = common_params.bytes_limit;
    output_surface_->SetMemoryPolicy(bytes_limit_);
  }
  if (total_scroll_offset_ != common_params.root_scroll_offset) {
    total_scroll_offset_ = common_params.root_scroll_offset;
    input_handler_proxy_->SynchronouslySetRootScrollOffset(
        total_scroll_offset_);
  }
  if (!common_params.ack.resources.empty()) {
    output_surface_->ReturnResources(common_params.ack);
  }
}

}  // namespace content

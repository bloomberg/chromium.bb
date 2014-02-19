// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/cross_process_frame_connector.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

CrossProcessFrameConnector::CrossProcessFrameConnector(
    RenderFrameHostImpl* frame_proxy_in_parent_renderer)
    : frame_proxy_in_parent_renderer_(frame_proxy_in_parent_renderer),
      view_(NULL),
      device_scale_factor_(1) {
  frame_proxy_in_parent_renderer->set_cross_process_frame_connector(this);
}

CrossProcessFrameConnector::~CrossProcessFrameConnector() {
  if (view_)
    view_->set_cross_process_frame_connector(NULL);
}

bool CrossProcessFrameConnector::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  bool msg_is_ok = true;

  IPC_BEGIN_MESSAGE_MAP_EX(CrossProcessFrameConnector, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(FrameHostMsg_BuffersSwappedACK, OnBuffersSwappedACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CompositorFrameSwappedACK,
                        OnCompositorFrameSwappedACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ReclaimCompositorResources,
                        OnReclaimCompositorResources)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ForwardInputEvent, OnForwardInputEvent)
    IPC_MESSAGE_HANDLER(FrameHostMsg_InitializeChildFrame,
                        OnInitializeChildFrame)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void CrossProcessFrameConnector::set_view(
    RenderWidgetHostViewChildFrame* view) {
  // Detach ourselves from the previous |view_|.
  if (view_)
    view_->set_cross_process_frame_connector(NULL);

  view_ = view;

  // Attach ourselves to the new view.
  if (view_)
    view_->set_cross_process_frame_connector(this);
}

void CrossProcessFrameConnector::RenderProcessGone() {
  frame_proxy_in_parent_renderer_->Send(new FrameMsg_ChildFrameProcessGone(
      frame_proxy_in_parent_renderer_->routing_id()));
}

void CrossProcessFrameConnector::ChildFrameBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& gpu_params,
    int gpu_host_id) {

  FrameMsg_BuffersSwapped_Params params;
  params.size = gpu_params.size;
  params.mailbox = gpu_params.mailbox;
  params.gpu_route_id = gpu_params.route_id;
  params.gpu_host_id = gpu_host_id;

  frame_proxy_in_parent_renderer_->Send(
      new FrameMsg_BuffersSwapped(
          frame_proxy_in_parent_renderer_->routing_id(),
          params));
}

void CrossProcessFrameConnector::ChildFrameCompositorFrameSwapped(
    uint32 output_surface_id,
    int host_id,
    int route_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  FrameMsg_CompositorFrameSwapped_Params params;
  frame->AssignTo(&params.frame);
  params.output_surface_id = output_surface_id;
  params.producing_route_id = route_id;
  params.producing_host_id = host_id;
  frame_proxy_in_parent_renderer_->Send(new FrameMsg_CompositorFrameSwapped(
      frame_proxy_in_parent_renderer_->routing_id(), params));
}

void CrossProcessFrameConnector::OnBuffersSwappedACK(
    const FrameHostMsg_BuffersSwappedACK_Params& params) {
  AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
  ack_params.mailbox = params.mailbox;
  ack_params.sync_point = params.sync_point;
  RenderWidgetHostImpl::AcknowledgeBufferPresent(params.gpu_route_id,
                                                 params.gpu_host_id,
                                                 ack_params);

  // TODO(kenrb): Special case stuff for Win + Mac.
}

void CrossProcessFrameConnector::OnCompositorFrameSwappedACK(
    const FrameHostMsg_CompositorFrameSwappedACK_Params& params) {
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(params.producing_route_id,
                                                   params.output_surface_id,
                                                   params.producing_host_id,
                                                   params.ack);
}

void CrossProcessFrameConnector::OnReclaimCompositorResources(
    const FrameHostMsg_ReclaimCompositorResources_Params& params) {
  RenderWidgetHostImpl::SendReclaimCompositorResources(params.route_id,
                                                       params.output_surface_id,
                                                       params.renderer_host_id,
                                                       params.ack);
}

void CrossProcessFrameConnector::OnInitializeChildFrame(gfx::Rect frame_rect,
                                                        float scale_factor) {
  if (scale_factor != device_scale_factor_) {
    device_scale_factor_ = scale_factor;
    if (view_) {
      RenderWidgetHostImpl* child_widget =
          RenderWidgetHostImpl::From(view_->GetRenderWidgetHost());
      child_widget->NotifyScreenInfoChanged();
    }
  }

  if (!frame_rect.size().IsEmpty()) {
    child_frame_rect_ = frame_rect;
    if (view_)
      view_->SetSize(frame_rect.size());
  }
}

gfx::Rect CrossProcessFrameConnector::ChildFrameRect() {
  return child_frame_rect_;
}

void CrossProcessFrameConnector::OnForwardInputEvent(
    const blink::WebInputEvent* event) {
  if (!view_)
    return;

  RenderWidgetHostImpl* child_widget =
      RenderWidgetHostImpl::From(view_->GetRenderWidgetHost());
  RenderWidgetHostImpl* parent_widget =
      frame_proxy_in_parent_renderer_->render_view_host();

  if (blink::WebInputEvent::isKeyboardEventType(event->type)) {
    if (!parent_widget->GetLastKeyboardEvent())
      return;
    NativeWebKeyboardEvent keyboard_event(
        *parent_widget->GetLastKeyboardEvent());
    child_widget->ForwardKeyboardEvent(keyboard_event);
    return;
  }

  if (blink::WebInputEvent::isMouseEventType(event->type)) {
    child_widget->ForwardMouseEvent(
        *static_cast<const blink::WebMouseEvent*>(event));
    return;
  }

  if (event->type == blink::WebInputEvent::MouseWheel) {
    child_widget->ForwardWheelEvent(
        *static_cast<const blink::WebMouseWheelEvent*>(event));
    return;
  }
}

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/cross_process_frame_connector.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/gpu_messages.h"

namespace content {

CrossProcessFrameConnector::CrossProcessFrameConnector(
    RenderFrameHostImpl* frame_proxy_in_parent_renderer)
    : frame_proxy_in_parent_renderer_(frame_proxy_in_parent_renderer),
      view_(NULL) {
    frame_proxy_in_parent_renderer->set_cross_process_frame_connector(this);
}

CrossProcessFrameConnector::~CrossProcessFrameConnector() {
  if (view_)
    view_->set_cross_process_child_frame(NULL);
}

bool CrossProcessFrameConnector::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  bool msg_is_ok = true;

  IPC_BEGIN_MESSAGE_MAP_EX(CrossProcessFrameConnector, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(FrameHostMsg_BuffersSwappedACK, OnBuffersSwappedACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void CrossProcessFrameConnector::SetView(
    RenderWidgetHostViewChildFrame* view) {
  // Detach ourselves from the previous |view_|.
  if (view_)
    view_->set_cross_process_child_frame(NULL);

  view_ = view;

  // Attach ourselves to the new view.
  if (view_)
    view_->set_cross_process_child_frame(this);
}

void CrossProcessFrameConnector::ChildFrameBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& gpu_params,
    int gpu_host_id) {

  FrameMsg_BuffersSwapped_Params params;
  params.size = gpu_params.size;
  params.mailbox_name = gpu_params.mailbox_name;
  params.gpu_route_id = gpu_params.route_id;
  params.gpu_host_id = gpu_host_id;

  frame_proxy_in_parent_renderer_->Send(
      new FrameMsg_BuffersSwapped(
          frame_proxy_in_parent_renderer_->routing_id(),
          params));
}

void CrossProcessFrameConnector::ChildFrameCompositorFrameSwapped(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
}

gfx::Rect CrossProcessFrameConnector::ChildFrameRect() {
  return child_frame_rect_;
}

void CrossProcessFrameConnector::OnBuffersSwappedACK(
    const FrameHostMsg_BuffersSwappedACK_Params& params) {
  AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
  ack_params.mailbox_name = params.mailbox_name;
  ack_params.sync_point = params.sync_point;
  RenderWidgetHostImpl::AcknowledgeBufferPresent(params.gpu_route_id,
                                                 params.gpu_host_id,
                                                 ack_params);

  // TODO(kenrb): Special case stuff for Win + Mac.
}

}  // namespace content

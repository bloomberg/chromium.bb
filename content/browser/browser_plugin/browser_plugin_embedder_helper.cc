// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder_helper.h"

#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/browser_plugin_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/size.h"

namespace content {

BrowserPluginEmbedderHelper::BrowserPluginEmbedderHelper(
    BrowserPluginEmbedder* embedder,
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      embedder_(embedder) {
}

BrowserPluginEmbedderHelper::~BrowserPluginEmbedderHelper() {
}

bool BrowserPluginEmbedderHelper::Send(IPC::Message* message) {
  return RenderViewHostObserver::Send(message);
}

bool BrowserPluginEmbedderHelper::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginEmbedderHelper, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_CreateGuest,
                        OnCreateGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_NavigateGuest,
                        OnNavigateGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ResizeGuest, OnResizeGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_UpdateRect_ACK, OnUpdateRectACK)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_HandleInputEvent,
                        OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_PluginDestroyed,
                        OnPluginDestroyed)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Go, OnGo)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Reload, OnReload)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_TerminateGuest, OnTerminateGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetVisibility,
                        OnSetGuestVisibility)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_DragStatusUpdate,
                        OnDragStatusUpdate)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetAutoSize, OnSetAutoSize)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_PluginAtPositionResponse,
                        OnPluginAtPositionResponse)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_BuffersSwappedACK,
                        OnSwapBuffersACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginEmbedderHelper::OnResizeGuest(
    int instance_id,
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  embedder_->ResizeGuest(render_view_host(), instance_id, params);
}

void BrowserPluginEmbedderHelper::OnHandleInputEvent(
    int instance_id,
    const gfx::Rect& guest_window_rect,
    const WebKit::WebInputEvent* input_event) {
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      render_view_host());
  embedder_->HandleInputEvent(instance_id,
                              rvh,
                              guest_window_rect,
                              *input_event);
}

void BrowserPluginEmbedderHelper::OnCreateGuest(
    int instance_id,
    const BrowserPluginHostMsg_CreateGuest_Params& params) {
  // The first BrowserPluginHostMsg_CreateGuest message is handled in
  // WebContentsImpl. All subsequent BrowserPluginHostMsg_CreateGuest
  // messages are handled here.
  embedder_->CreateGuest(render_view_host(), instance_id, params);
}

void BrowserPluginEmbedderHelper::OnNavigateGuest(
    int instance_id,
    const std::string& src) {
  embedder_->NavigateGuest(render_view_host(), instance_id, src);
}

void BrowserPluginEmbedderHelper::OnUpdateRectACK(
    int instance_id,
    const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params) {
  embedder_->UpdateRectACK(instance_id,
                           auto_size_params,
                           resize_guest_params);
}

void BrowserPluginEmbedderHelper::OnSwapBuffersACK(int route_id,
                                                   int gpu_host_id,
                                                   uint64 surface_handle,
                                                   uint32 sync_point) {
  AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
  ack_params.surface_handle = surface_handle;
  ack_params.sync_point = sync_point;
  RenderWidgetHostImpl::AcknowledgeBufferPresent(route_id,
                                                 gpu_host_id,
                                                 ack_params);
}

void BrowserPluginEmbedderHelper::OnSetFocus(int instance_id, bool focused) {
  embedder_->SetFocus(instance_id, focused);
}

void BrowserPluginEmbedderHelper::OnPluginAtPositionResponse(
    int instance_id, int request_id, const gfx::Point& location) {
  embedder_->PluginAtPositionResponse(instance_id, request_id, location);
}

void BrowserPluginEmbedderHelper::OnPluginDestroyed(int instance_id) {
  embedder_->PluginDestroyed(instance_id);
}

void BrowserPluginEmbedderHelper::OnGo(int instance_id, int relative_index) {
  embedder_->Go(instance_id, relative_index);
}

void BrowserPluginEmbedderHelper::OnStop(int instance_id) {
  embedder_->Stop(instance_id);
}

void BrowserPluginEmbedderHelper::OnReload(int instance_id) {
  embedder_->Reload(instance_id);
}

void BrowserPluginEmbedderHelper::OnTerminateGuest(int instance_id) {
  embedder_->TerminateGuest(instance_id);
}

void BrowserPluginEmbedderHelper::OnSetGuestVisibility(int instance_id,
                                                       bool visible) {
  embedder_->SetGuestVisibility(instance_id, visible);
}

void BrowserPluginEmbedderHelper::OnDragStatusUpdate(
    int instance_id,
    WebKit::WebDragStatus drag_status,
    const WebDropData& drop_data,
    WebKit::WebDragOperationsMask drag_mask,
    const gfx::Point& location) {
  embedder_->DragStatusUpdate(instance_id, drag_status, drop_data, drag_mask,
      location);
}

void BrowserPluginEmbedderHelper::OnSetAutoSize(
    int instance_id,
    const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params) {
  embedder_->SetAutoSize(instance_id, auto_size_params, resize_guest_params);
}

}  // namespace content

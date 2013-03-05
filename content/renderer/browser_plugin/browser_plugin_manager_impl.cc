// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_manager_impl.h"

#include "content/common/browser_plugin_messages.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/render_thread_impl.h"
#include "ui/gfx/point.h"
#include "webkit/glue/webcursor.h"

namespace content {

BrowserPluginManagerImpl::BrowserPluginManagerImpl(
    RenderViewImpl* render_view)
    : BrowserPluginManager(render_view),
      request_id_counter_(0) {
}

BrowserPluginManagerImpl::~BrowserPluginManagerImpl() {
}

BrowserPlugin* BrowserPluginManagerImpl::CreateBrowserPlugin(
    RenderViewImpl* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params) {
  return new BrowserPlugin(render_view, frame, params);
}

void BrowserPluginManagerImpl::AllocateInstanceID(
    BrowserPlugin* browser_plugin) {
  int request_id = request_id_counter_++;
  pending_allocate_instance_id_requests_.AddWithID(browser_plugin, request_id);
  Send(new BrowserPluginHostMsg_AllocateInstanceID(
      browser_plugin->render_view_routing_id(), request_id));
}

bool BrowserPluginManagerImpl::Send(IPC::Message* msg) {
  return RenderThread::Get()->Send(msg);
}

bool BrowserPluginManagerImpl::OnMessageReceived(
    const IPC::Message& message) {
  if (ShouldForwardToBrowserPlugin(message)) {
    int instance_id = 0;
    // All allowed messages must have instance_id as their first parameter.
    PickleIterator iter(message);
    bool success = iter.ReadInt(&instance_id);
    DCHECK(success);
    BrowserPlugin* plugin = GetBrowserPlugin(instance_id);
    if (plugin && plugin->OnMessageReceived(message))
      return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginManagerImpl, message)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_AllocateInstanceID_ACK,
                        OnAllocateInstanceIDACK)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_BuffersSwapped,
                        OnUnhandledSwap);
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_PluginAtPositionRequest,
                        OnPluginAtPositionRequest);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginManagerImpl::OnAllocateInstanceIDACK(
    const IPC::Message& message, int request_id, int instance_id) {
  BrowserPlugin* plugin =
      pending_allocate_instance_id_requests_.Lookup(request_id);
  pending_allocate_instance_id_requests_.Remove(request_id);
  if (plugin)
    plugin->SetInstanceID(instance_id);
}

void BrowserPluginManagerImpl::OnPluginAtPositionRequest(
    const IPC::Message& message,
    int request_id,
    const gfx::Point& position) {
  int instance_id = -1;
  IDMap<BrowserPlugin>::iterator it(&instances_);
  gfx::Point local_position = position;
  while (!it.IsAtEnd()) {
    const BrowserPlugin* plugin = it.GetCurrentValue();
    if (plugin->InBounds(position)) {
      instance_id = plugin->instance_id();
      local_position = plugin->ToLocalCoordinates(position);
      break;
    }
    it.Advance();
  }

  Send(new BrowserPluginHostMsg_PluginAtPositionResponse(
       message.routing_id(), instance_id, request_id, local_position));
}

void BrowserPluginManagerImpl::OnUnhandledSwap(const IPC::Message& message,
                                               int instance_id,
                                               const gfx::Size& size,
                                               std::string mailbox_name,
                                               int gpu_route_id,
                                               int gpu_host_id) {
  // After the BrowserPlugin object sends a destroy message to the
  // guest, it goes away and is unable to handle messages that
  // might still be coming from the guest.
  // In this case, we might receive a BuffersSwapped message that
  // we need to ACK.
  // Issue is tracked in crbug.com/170745.
  Send(new BrowserPluginHostMsg_BuffersSwappedACK(
      message.routing_id(),
      instance_id,
      gpu_route_id,
      gpu_host_id,
      mailbox_name,
      0));
}

// static
bool BrowserPluginManagerImpl::ShouldForwardToBrowserPlugin(
    const IPC::Message& message) {
  switch (message.type()) {
    case BrowserPluginMsg_AdvanceFocus::ID:
    case BrowserPluginMsg_BuffersSwapped::ID:
    case BrowserPluginMsg_GuestContentWindowReady::ID:
    case BrowserPluginMsg_GuestGone::ID:
    case BrowserPluginMsg_GuestResponsive::ID:
    case BrowserPluginMsg_GuestUnresponsive::ID:
    case BrowserPluginMsg_LoadAbort::ID:
    case BrowserPluginMsg_LoadCommit::ID:
    case BrowserPluginMsg_LoadRedirect::ID:
    case BrowserPluginMsg_LoadStart::ID:
    case BrowserPluginMsg_LoadStop::ID:
    case BrowserPluginMsg_LockMouse::ID:
    case BrowserPluginMsg_RequestPermission::ID:
    case BrowserPluginMsg_SetCursor::ID:
    case BrowserPluginMsg_ShouldAcceptTouchEvents::ID:
    case BrowserPluginMsg_UnlockMouse::ID:
    case BrowserPluginMsg_UpdatedName::ID:
    case BrowserPluginMsg_UpdateRect::ID:
      return true;
    default:
      break;
  }
  return false;
}

}  // namespace content

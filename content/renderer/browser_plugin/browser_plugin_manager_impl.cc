// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_manager_impl.h"

#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/render_thread_impl.h"
#include "ui/gfx/point.h"
#include "webkit/common/cursors/webcursor.h"

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
  int request_id = ++request_id_counter_;
  pending_allocate_guest_instance_id_requests_.AddWithID(browser_plugin,
                                                         request_id);
  Send(new BrowserPluginHostMsg_AllocateInstanceID(
      browser_plugin->render_view_routing_id(), request_id));
}

bool BrowserPluginManagerImpl::Send(IPC::Message* msg) {
  return RenderThread::Get()->Send(msg);
}

bool BrowserPluginManagerImpl::OnMessageReceived(
    const IPC::Message& message) {
  if (BrowserPlugin::ShouldForwardToBrowserPlugin(message)) {
    int guest_instance_id = browser_plugin::kInstanceIDNone;
    // All allowed messages must have |guest_instance_id| as their first
    // parameter.
    PickleIterator iter(message);
    bool success = iter.ReadInt(&guest_instance_id);
    DCHECK(success);
    BrowserPlugin* plugin = GetBrowserPlugin(guest_instance_id);
    if (plugin && plugin->OnMessageReceived(message))
      return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginManagerImpl, message)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_AllocateInstanceID_ACK,
                        OnAllocateInstanceIDACK)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_PluginAtPositionRequest,
                        OnPluginAtPositionRequest);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginManagerImpl::DidCommitCompositorFrame() {
  IDMap<BrowserPlugin>::iterator iter(&instances_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->DidCommitCompositorFrame();
    iter.Advance();
  }
}

void BrowserPluginManagerImpl::OnAllocateInstanceIDACK(
    const IPC::Message& message,
    int request_id,
    int guest_instance_id) {
  BrowserPlugin* plugin =
    pending_allocate_guest_instance_id_requests_.Lookup(request_id);
  if (!plugin)
    return;
  pending_allocate_guest_instance_id_requests_.Remove(request_id);
  plugin->OnInstanceIDAllocated(guest_instance_id);
}

void BrowserPluginManagerImpl::OnPluginAtPositionRequest(
    const IPC::Message& message,
    int request_id,
    const gfx::Point& position) {
  int guest_instance_id = browser_plugin::kInstanceIDNone;
  IDMap<BrowserPlugin>::iterator it(&instances_);
  gfx::Point local_position = position;
  while (!it.IsAtEnd()) {
    const BrowserPlugin* plugin = it.GetCurrentValue();
    if (!plugin->guest_crashed() && plugin->InBounds(position)) {
      guest_instance_id = plugin->guest_instance_id();
      local_position = plugin->ToLocalCoordinates(position);
      break;
    }
    it.Advance();
  }

  Send(new BrowserPluginHostMsg_PluginAtPositionResponse(
       message.routing_id(), guest_instance_id, request_id, local_position));
}

}  // namespace content

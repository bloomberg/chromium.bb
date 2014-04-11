// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_manager_impl.h"

#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/cursors/webcursor.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/render_thread_impl.h"
#include "ui/gfx/point.h"

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
    blink::WebFrame* frame,
    bool auto_navigate) {
  return new BrowserPlugin(render_view, frame, auto_navigate);
}

void BrowserPluginManagerImpl::AllocateInstanceID(
    const base::WeakPtr<BrowserPlugin>& browser_plugin) {
  int request_id = ++request_id_counter_;
  pending_allocate_guest_instance_id_requests_.insert(
      std::make_pair(request_id, browser_plugin));
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
  InstanceIDMap::iterator it =
      pending_allocate_guest_instance_id_requests_.find(request_id);
  if (it == pending_allocate_guest_instance_id_requests_.end())
    return;

  const base::WeakPtr<BrowserPlugin> plugin(it->second);
  if (!plugin)
    return;
  pending_allocate_guest_instance_id_requests_.erase(request_id);
  plugin->OnInstanceIDAllocated(guest_instance_id);
}

}  // namespace content

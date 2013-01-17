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
    : BrowserPluginManager(render_view) {
}

BrowserPluginManagerImpl::~BrowserPluginManagerImpl() {
}

BrowserPlugin* BrowserPluginManagerImpl::CreateBrowserPlugin(
    RenderViewImpl* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params) {
  return new BrowserPlugin(++browser_plugin_counter_,
                           render_view,
                           frame,
                           params);
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
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_PluginAtPositionRequest,
                        OnPluginAtPositionRequest);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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

// static
bool BrowserPluginManagerImpl::ShouldForwardToBrowserPlugin(
    const IPC::Message& message) {
  switch (message.type()) {
    case BrowserPluginMsg_AdvanceFocus::ID:
    case BrowserPluginMsg_GuestContentWindowReady::ID:
    case BrowserPluginMsg_GuestGone::ID:
    case BrowserPluginMsg_GuestResponsive::ID:
    case BrowserPluginMsg_GuestUnresponsive::ID:
    case BrowserPluginMsg_LoadAbort::ID:
    case BrowserPluginMsg_LoadCommit::ID:
    case BrowserPluginMsg_LoadRedirect::ID:
    case BrowserPluginMsg_LoadStart::ID:
    case BrowserPluginMsg_LoadStop::ID:
    case BrowserPluginMsg_SetCursor::ID:
    case BrowserPluginMsg_ShouldAcceptTouchEvents::ID:
    case BrowserPluginMsg_UpdatedName::ID:
    case BrowserPluginMsg_UpdateRect::ID:
    case BrowserPluginMsg_BuffersSwapped::ID:
      return true;
    default:
      break;
  }
  return false;
}

}  // namespace content

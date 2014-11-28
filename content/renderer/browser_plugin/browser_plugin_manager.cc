// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_manager.h"

#include "base/memory/scoped_ptr.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/frame_messages.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "ipc/ipc_message_macros.h"

namespace content {

// static
BrowserPluginManager* BrowserPluginManager::Create(
    RenderViewImpl* render_view) {
  return new BrowserPluginManager(render_view);
}

BrowserPluginManager::BrowserPluginManager(RenderViewImpl* render_view)
    : RenderViewObserver(render_view),
      current_instance_id_(browser_plugin::kInstanceIDNone),
      render_view_(render_view->AsWeakPtr()) {
}

BrowserPluginManager::~BrowserPluginManager() {
}

void BrowserPluginManager::AddBrowserPlugin(
    int browser_plugin_instance_id,
    BrowserPlugin* browser_plugin) {
  instances_.AddWithID(browser_plugin, browser_plugin_instance_id);
}

void BrowserPluginManager::RemoveBrowserPlugin(int browser_plugin_instance_id) {
  instances_.Remove(browser_plugin_instance_id);
}

BrowserPlugin* BrowserPluginManager::GetBrowserPlugin(
    int browser_plugin_instance_id) const {
  return instances_.Lookup(browser_plugin_instance_id);
}

int BrowserPluginManager::GetNextInstanceID() {
  return ++current_instance_id_;
}

void BrowserPluginManager::UpdateDeviceScaleFactor() {
  IDMap<BrowserPlugin>::iterator iter(&instances_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->UpdateDeviceScaleFactor();
    iter.Advance();
  }
}

void BrowserPluginManager::UpdateFocusState() {
  IDMap<BrowserPlugin>::iterator iter(&instances_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->UpdateGuestFocusState();
    iter.Advance();
  }
}

void BrowserPluginManager::Attach(int browser_plugin_instance_id) {
  BrowserPlugin* plugin = GetBrowserPlugin(browser_plugin_instance_id);
  if (plugin)
    plugin->Attach();
}

BrowserPlugin* BrowserPluginManager::CreateBrowserPlugin(
    RenderViewImpl* render_view,
    blink::WebFrame* frame,
    scoped_ptr<BrowserPluginDelegate> delegate) {
  return new BrowserPlugin(render_view, frame, delegate.Pass());
}

void BrowserPluginManager::DidCommitCompositorFrame() {
  IDMap<BrowserPlugin>::iterator iter(&instances_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->DidCommitCompositorFrame();
    iter.Advance();
  }
}

bool BrowserPluginManager::OnMessageReceived(
    const IPC::Message& message) {
  if (!BrowserPlugin::ShouldForwardToBrowserPlugin(message))
    return false;

  int browser_plugin_instance_id = browser_plugin::kInstanceIDNone;
  // All allowed messages must have |browser_plugin_instance_id| as their
  // first parameter.
  PickleIterator iter(message);
  bool success = iter.ReadInt(&browser_plugin_instance_id);
  DCHECK(success);
  BrowserPlugin* plugin = GetBrowserPlugin(browser_plugin_instance_id);
  if (plugin && plugin->OnMessageReceived(message))
    return true;

  // TODO(fsamuel): This is probably forcing the compositor to continue working
  // even on display:none. We should optimize this.
  if (message.type() == BrowserPluginMsg_CompositorFrameSwapped::ID) {
    OnCompositorFrameSwappedPluginUnavailable(message);
    return true;
  }

  return false;
}

bool BrowserPluginManager::Send(IPC::Message* msg) {
  return RenderThread::Get()->Send(msg);
}

void BrowserPluginManager::OnCompositorFrameSwappedPluginUnavailable(
    const IPC::Message& message) {
  BrowserPluginMsg_CompositorFrameSwapped::Param param;
  if (!BrowserPluginMsg_CompositorFrameSwapped::Read(&message, &param))
    return;

  FrameHostMsg_CompositorFrameSwappedACK_Params params;
  params.producing_host_id = param.b.producing_host_id;
  params.producing_route_id = param.b.producing_route_id;
  params.output_surface_id = param.b.output_surface_id;
  Send(new BrowserPluginHostMsg_CompositorFrameSwappedACK(
      routing_id(), param.a, params));
}

}  // namespace content

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_channel_manager.h"

#include "base/process_util.h"
#include "content/common/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/guest_to_embedder_channel.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "ppapi/c/pp_instance.h"

namespace content {

BrowserPluginChannelManager::BrowserPluginChannelManager() {
}

BrowserPluginChannelManager::~BrowserPluginChannelManager() {
}

void BrowserPluginChannelManager::CreateRenderView(
    const ViewMsg_New_Params& params) {
  IPC::ChannelHandle plugin_handle;
  plugin_handle.name =
      IPC::Channel::GenerateVerifiedChannelID(params.embedder_channel_name);
  bool success = true;
  scoped_refptr<GuestToEmbedderChannel> channel =
      GetChannelByName(params.embedder_channel_name);
  if (!channel) {
    channel = new GuestToEmbedderChannel(params.embedder_channel_name);
    success = channel->InitChannel(plugin_handle);

#if defined(OS_POSIX)
    // On POSIX, transfer ownership of the renderer-side (client) FD.
    // This ensures this process will be notified when it is closed even if a
    // connection is not established.
    plugin_handle.socket =
        base::FileDescriptor(channel->TakeRendererFD(), true);
    if (plugin_handle.socket.fd == -1)
      success = false;
#endif
    DCHECK(success);
    embedder_channels_[params.embedder_channel_name] = channel;
  }
  DCHECK(pending_guests_.find(params.view_id) ==
         pending_guests_.end());
  pending_guests_[params.view_id] =
    RenderViewImpl::Create(
        params.parent_window,
        params.opener_route_id,
        params.renderer_preferences,
        params.web_preferences,
        new SharedRenderViewCounter(0),
        params.view_id,
        params.surface_id,
        params.session_storage_namespace_id,
        params.frame_name,
        false,
        params.swapped_out,
        params.next_page_id,
        params.screen_info,
        channel,
        params.accessibility_mode)->AsWeakPtr();
  RenderThreadImpl::current()->Send(
      new BrowserPluginHostMsg_ConnectToChannel(params.view_id,
          success ? plugin_handle : IPC::ChannelHandle()));
}

bool BrowserPluginChannelManager::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginChannelManager, message)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_CompleteNavigation,
                        OnCompleteNavigation)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_LoadGuest, OnLoadGuest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

GuestToEmbedderChannel* BrowserPluginChannelManager::GetChannelByName(
    const std::string& embedder_channel_name) {
  EmbedderChannelNameToChannelMap::iterator it =
      embedder_channels_.find(embedder_channel_name);
  if (it != embedder_channels_.end())
    return it->second;
  return NULL;
}

void BrowserPluginChannelManager::RemoveChannelByName(
    const std::string& embedder_channel_name) {
  embedder_channels_.erase(embedder_channel_name);
}

void BrowserPluginChannelManager::OnCompleteNavigation(
    int guest_routing_id,
    PP_Instance instance) {
  CHECK(pending_guests_.find(guest_routing_id) !=
        pending_guests_.end());
  RenderViewImpl* render_view = pending_guests_[guest_routing_id];
  pending_guests_.erase(guest_routing_id);
  GuestToEmbedderChannel* channel = render_view->guest_to_embedder_channel();
  // Associate the RenderView with the provided PP_Instance ID, request the
  // receipt of events, and initialize the graphics context.
  channel->AddGuest(instance, render_view);
  channel->RequestInputEvents(instance);
  render_view->GuestReady(instance);
}

void BrowserPluginChannelManager::OnLoadGuest(
    int instance_id,
    int guest_process_id,
    const IPC::ChannelHandle& channel_handle) {
  BrowserPlugin* browser_plugin = BrowserPlugin::FromID(instance_id);
  browser_plugin->LoadGuest(guest_process_id, channel_handle);
}

}  // namespace content

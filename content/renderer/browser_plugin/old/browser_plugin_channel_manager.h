// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_CHANNEL_MANAGER_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_CHANNEL_MANAGER_H_
#pragma once

#include <set>

#include "base/id_map.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/renderer/browser_plugin/old/guest_to_embedder_channel.h"
#include "content/renderer/render_view_impl.h"

class GuestToEmbedderChannel;
struct ViewMsg_New_Params;

namespace content {

// BrowserPluginChannelManager manages the lifetime of GuestToEmbedderChannels.
// When a new RenderView is requested, it checks the embedder channel name
// in ViewMsg_New_Params and decides whether to reuse an existing channel or
// create a new channel. On the guest renderer process, it informs a
// RenderView once a channel has been established with its embedder.
// On the embedder side, it tells BrowserPlugin to load the guest
// PluginInstance once a channel has been established.
class BrowserPluginChannelManager
    : public RenderProcessObserver {
 public:
  BrowserPluginChannelManager();

  virtual ~BrowserPluginChannelManager();

  void CreateRenderView(const ViewMsg_New_Params& params);

  void ReportChannelToEmbedder(
      RenderViewImpl* render_view,
      const IPC::ChannelHandle& embedder_channel_handle,
      const std::string& embedder_channel_name,
      int embedder_container_id);

  // Get the GuestToEmbedderChannel associated with the given
  // embedder_channel_name.
  GuestToEmbedderChannel* GetChannelByName(
      const std::string& embedder_channel_name);

  // Remove the pointer to the GuestToEmbedderChannel associated with the given
  // routing_id.
  void RemoveChannelByName(const std::string& embedder_channel_name);

  void GuestReady(PP_Instance instance,
                  const std::string& embedder_channel_name,
                  int embedder_container_id);

 private:
  typedef std::map<std::string, scoped_refptr<GuestToEmbedderChannel> >
      EmbedderChannelNameToChannelMap;

  void OnLoadGuest(int instance_id,
                   int guest_renderer_id,
                   const IPC::ChannelHandle& channel_handle);
  void OnAdvanceFocus(int instance_id, bool reverse);

  // RenderProcessObserver override. Call on render thread.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  // Map from Host process ID to GuestToEmbedderChannel
  EmbedderChannelNameToChannelMap embedder_channels_;

  // Map from <embedder_channel_name, embedder_container_id> to RenderViewImpl
  // that points to RenderViewImpl guests that have been constructed but don't
  // have a PP_Instance and so they aren't yet ready to composite.
  std::map<std::pair<std::string, int>,
           base::WeakPtr<RenderViewImpl> > pending_guests_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginChannelManager);
};

}  // namespace content

#endif //  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_CHANNEL_MANAGER_H_

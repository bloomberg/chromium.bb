// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_HOST_HELPER_H__
#define CONTENT_BROWSER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_HOST_HELPER_H__
#pragma once

#include <string>

#include "content/public/browser/render_view_host_observer.h"

namespace gfx {
class Size;
}

namespace content {

class BrowserPluginHost;

// This class acts as a plumber helper for BrowserPluginHost. A lot
// of messages coming from guests need to know the guest's RenderViewHost.
// BrowserPluginHostHelper handles BrowserPluginHost messages and relays
// them with their associated RenderViewHosts to BrowserPluginHost where they
// will be handled.
class BrowserPluginHostHelper : public RenderViewHostObserver {
 public:
  BrowserPluginHostHelper(BrowserPluginHost* browser_plugin_host,
                          RenderViewHost* render_view_host);
  virtual ~BrowserPluginHostHelper();

 private:
  void OnConnectToChannel(const IPC::ChannelHandle& channel_handle);
  void OnNavigateGuestFromEmbedder(int container_instance_id,
                                   long long frame_id,
                                   const std::string& src);
  void OnResizeGuest(int width, int height);

  // RenderViewHostObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  BrowserPluginHost* browser_plugin_host_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginHostHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_HOST_HELPER_H__


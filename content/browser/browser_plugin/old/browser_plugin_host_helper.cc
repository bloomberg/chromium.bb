// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/old/browser_plugin_host_helper.h"

#include "content/browser/browser_plugin/old/browser_plugin_host.h"
#include "content/common/browser_plugin_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/size.h"

namespace content {

BrowserPluginHostHelper::BrowserPluginHostHelper(
    BrowserPluginHost* browser_plugin_host,
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      browser_plugin_host_(browser_plugin_host) {
}

BrowserPluginHostHelper::~BrowserPluginHostHelper() {
}


bool BrowserPluginHostHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginHostHelper, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ConnectToChannel,
                        OnConnectToChannel)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_NavigateFromEmbedder,
                        OnNavigateGuestFromEmbedder)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ResizeGuest, OnResizeGuest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginHostHelper::OnConnectToChannel(
    const IPC::ChannelHandle& channel_handle) {
  browser_plugin_host_->ConnectEmbedderToChannel(
      render_view_host(),
      channel_handle);
}

void BrowserPluginHostHelper::OnNavigateGuestFromEmbedder(
    int32 instance_id,
    long long frame_id,
    const std::string& src) {
  browser_plugin_host_->NavigateGuestFromEmbedder(
      render_view_host(),
      instance_id,
      frame_id,
      src);
}

void BrowserPluginHostHelper::OnResizeGuest(int width, int height) {
  render_view_host()->GetView()->SetSize(gfx::Size(width, height));
}

}  // namespace content

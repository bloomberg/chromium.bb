// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/mock_browser_plugin_manager.h"

#include "ipc/ipc_message.h"
#include "content/renderer/browser_plugin/mock_browser_plugin.h"

namespace content {

MockBrowserPluginManager::MockBrowserPluginManager(
    RenderViewImpl* render_view)
    : BrowserPluginManager(render_view) {
}

MockBrowserPluginManager::~MockBrowserPluginManager() {
}

BrowserPlugin* MockBrowserPluginManager::CreateBrowserPlugin(
    RenderViewImpl* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params) {
  return new MockBrowserPlugin(++browser_plugin_counter_,
                               render_view,
                               frame,
                               params);
}

bool MockBrowserPluginManager::Send(IPC::Message* msg) {
  // This is a copy-and-paste from MockRenderThread::Send.
  // We need to simulate a synchronous channel, thus we are going to receive
  // through this function messages, messages with reply and reply messages.
  // We can only handle one synchronous message at a time.
  if (msg->is_reply()) {
    if (reply_deserializer_.get()) {
      reply_deserializer_->SerializeOutputParameters(*msg);
      reply_deserializer_.reset();
    }
  } else {
    if (msg->is_sync()) {
      // We actually need to handle deleting the reply deserializer for sync
      // messages.
      reply_deserializer_.reset(
          static_cast<IPC::SyncMessage*>(msg)->GetReplyDeserializer());
    }
    OnMessageReceived(*msg);
  }
  delete msg;
  return true;
}

bool MockBrowserPluginManager::OnMessageReceived(
    const IPC::Message& message) {
  // Save the message in the sink.
  sink_.OnMessageReceived(message);
  return false;
}

}  // namespace content

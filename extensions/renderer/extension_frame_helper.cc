// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_frame_helper.h"

#include "content/public/renderer/render_frame.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/messaging_bindings.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

ExtensionFrameHelper::ExtensionFrameHelper(content::RenderFrame* render_frame,
                                           Dispatcher* extension_dispatcher)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<ExtensionFrameHelper>(render_frame),
      extension_dispatcher_(extension_dispatcher) {}

ExtensionFrameHelper::~ExtensionFrameHelper() {
}

void ExtensionFrameHelper::WillReleaseScriptContext(
    v8::Handle<v8::Context> context,
    int world_id) {
  extension_dispatcher_->WillReleaseScriptContext(
      render_frame()->GetWebFrame(), context, world_id);
}

bool ExtensionFrameHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionFrameHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AddMessageToConsole,
                        OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnConnect,
                        OnExtensionDispatchOnConnect)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnExtensionDeliverMessage)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnDisconnect,
                        OnExtensionDispatchOnDisconnect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionFrameHelper::OnAddMessageToConsole(
    content::ConsoleMessageLevel level,
    const std::string& message) {
  console::AddMessage(render_frame()->GetRenderView(), level, message);
}

void ExtensionFrameHelper::OnExtensionDispatchOnConnect(
    int target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo& source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id) {
  MessagingBindings::DispatchOnConnect(
      extension_dispatcher_->script_context_set(),
      target_port_id,
      channel_name,
      source,
      info,
      tls_channel_id,
      render_frame());
}

void ExtensionFrameHelper::OnExtensionDeliverMessage(int target_id,
                                                     const Message& message) {
  MessagingBindings::DeliverMessage(
      extension_dispatcher_->script_context_set(), target_id, message,
      render_frame());
}

void ExtensionFrameHelper::OnExtensionDispatchOnDisconnect(
    int port_id,
    const std::string& error_message) {
  MessagingBindings::DispatchOnDisconnect(
      extension_dispatcher_->script_context_set(), port_id, error_message,
      render_frame());
}

}  // namespace extensions

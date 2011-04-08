// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_handler.h"

#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"

ExtensionMessageHandler::ExtensionMessageHandler(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host) {
}

ExtensionMessageHandler::~ExtensionMessageHandler() {
}

bool ExtensionMessageHandler::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionMessageHandler, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionMessageHandler::OnPostMessage(int port_id,
                                            const std::string& message) {
  Profile* profile = render_view_host()->process()->profile();
  if (profile->GetExtensionMessageService()) {
    profile->GetExtensionMessageService()->PostMessageFromRenderer(
        port_id, message);
  }
}

void ExtensionMessageHandler::OnRequest(
    const ExtensionHostMsg_DomMessage_Params& params) {
  if (!ChildProcessSecurityPolicy::GetInstance()->
          HasExtensionBindings(render_view_host()->process()->id())) {
    // This can happen if someone uses window.open() to open an extension URL
    // from a non-extension context.
    Send(new ExtensionMsg_Response(
        routing_id(), params.request_id, false, std::string(),
        "Access to extension API denied."));
    return;
  }

  render_view_host()->delegate()->ProcessWebUIMessage(params);
}

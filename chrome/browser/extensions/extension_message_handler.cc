// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_handler.h"

#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/public/browser/render_process_host.h"

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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionMessageHandler::RenderViewHostInitialized() {
  Send(new ExtensionMsg_NotifyRenderViewType(
      routing_id(), render_view_host()->delegate()->GetRenderViewType()));
}

void ExtensionMessageHandler::OnPostMessage(int port_id,
                                            const std::string& message) {
  Profile* profile = Profile::FromBrowserContext(
      render_view_host()->process()->GetBrowserContext());
  if (profile->GetExtensionMessageService()) {
    profile->GetExtensionMessageService()->PostMessageFromRenderer(
        port_id, message);
  }
}

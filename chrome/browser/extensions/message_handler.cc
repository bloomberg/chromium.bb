// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/message_handler.h"

#include "chrome/browser/extensions/api/messaging/message_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"

using content::WebContents;

namespace extensions {

MessageHandler::MessageHandler(
    content::RenderViewHost* render_view_host)
    : content::RenderViewHostObserver(render_view_host) {
}

MessageHandler::~MessageHandler() {
}

bool MessageHandler::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MessageHandler, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MessageHandler::RenderViewHostInitialized() {
  WebContents* web_contents =
      WebContents::FromRenderViewHost(render_view_host());
  Send(new ExtensionMsg_NotifyRenderViewType(
      routing_id(), extensions::GetViewType(web_contents)));
}

void MessageHandler::OnPostMessage(int port_id,
                                            const std::string& message) {
  Profile* profile = Profile::FromBrowserContext(
      render_view_host()->GetProcess()->GetBrowserContext());
  MessageService* message_service =
      ExtensionSystem::Get(profile)->message_service();
  if (message_service) {
    message_service->PostMessage(port_id, message);
  }
}

}  // namespace extensions

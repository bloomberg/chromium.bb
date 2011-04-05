// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_handler.h"

#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"

ExtensionMessageHandler::ExtensionMessageHandler(
    int child_id, IPC::Message::Sender* sender, Profile* profile)
    : child_id_(child_id),
      sender_(sender),
      extension_function_dispatcher_(NULL),
      profile_(profile) {
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

bool ExtensionMessageHandler::CanDispatchRequest(
    int child_id,
    int routing_id,
    const ExtensionHostMsg_DomMessage_Params& params) {
  if (!ChildProcessSecurityPolicy::GetInstance()->
          HasExtensionBindings(child_id)) {
    // This can happen if someone uses window.open() to open an extension URL
    // from a non-extension context.
    sender_->Send(new ExtensionMsg_Response(
        routing_id, params.request_id, false, std::string(),
        "Access to extension API denied."));
    return false;
  }

  return true;
}

void ExtensionMessageHandler::OnPostMessage(int port_id,
                                            const std::string& message) {
  if (profile_->GetExtensionMessageService()) {
    profile_->GetExtensionMessageService()->PostMessageFromRenderer(
        port_id, message);
  }
}

void ExtensionMessageHandler::OnRequest(
    const IPC::Message& message,
    const ExtensionHostMsg_DomMessage_Params& params) {
  DCHECK(child_id_);
  if (!extension_function_dispatcher_ ||
      !CanDispatchRequest(child_id_, message.routing_id(), params)) {
    return;
  }

  extension_function_dispatcher_->HandleRequest(params);
}

ExtensionMessageObserver::ExtensionMessageObserver(TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          handler_(0, this, tab_contents->profile())) {
  // We don't pass in a child_id to handler_ since for TabContents that can
  // change later.
}

ExtensionMessageObserver::~ExtensionMessageObserver() {
}

bool ExtensionMessageObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionMessageObserver, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled)
    handled = handler_.OnMessageReceived(message);
  return handled;
}

void ExtensionMessageObserver::OnRequest(
    const IPC::Message& message,
    const ExtensionHostMsg_DomMessage_Params& params) {
  if (!handler_.CanDispatchRequest(
          tab_contents()->render_view_host()->process()->id(),
          message.routing_id(),
          params)) {
    return;
  }

  tab_contents()->render_view_host()->delegate()->ProcessWebUIMessage(params);
}

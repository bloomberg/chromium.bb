// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_session.h"

#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/devtools/protocol/cast_handler.h"
#include "chrome/browser/devtools/protocol/page_handler.h"
#include "chrome/browser/devtools/protocol/target_handler.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_manager_delegate.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/devtools/protocol/window_manager_handler.h"
#endif

ChromeDevToolsSession::ChromeDevToolsSession(
    content::DevToolsAgentHost* agent_host,
    content::DevToolsAgentHostClient* client)
    : agent_host_(agent_host),
      client_(client),
      dispatcher_(std::make_unique<protocol::UberDispatcher>(this)) {
  if (agent_host->GetWebContents() &&
      agent_host->GetType() == content::DevToolsAgentHost::kTypePage) {
    page_handler_ = std::make_unique<PageHandler>(agent_host->GetWebContents(),
                                                  dispatcher_.get());
    if (client->MayAttachToBrowser()) {
      cast_handler_ = std::make_unique<CastHandler>(
          agent_host->GetWebContents(), dispatcher_.get());
    }
  }
  target_handler_ = std::make_unique<TargetHandler>(dispatcher_.get());
  if (client->MayAttachToBrowser()) {
    browser_handler_ = std::make_unique<BrowserHandler>(dispatcher_.get(),
                                                        agent_host->GetId());
  }
#if defined(OS_CHROMEOS)
  window_manager_protocl_handler_ =
      std::make_unique<WindowManagerHandler>(dispatcher_.get());
#endif
}

ChromeDevToolsSession::~ChromeDevToolsSession() = default;

void ChromeDevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  pending_commands_.erase(call_id);
  client_->DispatchProtocolMessage(agent_host_, message->serialize());
}

void ChromeDevToolsSession::HandleCommand(
    std::unique_ptr<base::DictionaryValue> command_dict,
    const std::string& message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  int call_id;
  std::string method;
  std::unique_ptr<protocol::Value> protocolCommand =
      protocol::toProtocolValue(command_dict.get(), 1000);
  if (!dispatcher_->parseCommand(protocolCommand.get(), &call_id, &method))
    return;
  if (dispatcher_->canDispatch(method)) {
    pending_commands_[call_id] =
        std::make_pair(std::move(callback), std::move(command_dict));
    dispatcher_->dispatch(call_id, method, std::move(protocolCommand), message);
    return;
  }
  std::move(callback).Run(std::move(command_dict), message);
}

void ChromeDevToolsSession::fallThrough(int call_id,
                                        const std::string& method,
                                        const std::string& message) {
  PendingCommand command = std::move(pending_commands_[call_id]);
  pending_commands_.erase(call_id);
  std::move(command.first).Run(std::move(command.second), message);
}

void ChromeDevToolsSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  client_->DispatchProtocolMessage(agent_host_, message->serialize());
}

void ChromeDevToolsSession::flushProtocolNotifications() {}

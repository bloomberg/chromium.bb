// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_browser_target.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "net/server/http_server.h"

namespace content {

DevToolsBrowserTarget::DomainHandler::~DomainHandler() {
}

void DevToolsBrowserTarget::DomainHandler::RegisterCommandHandler(
    const std::string& command,
    CommandHandler handler) {
  command_handlers_[command] = handler;
}

DevToolsBrowserTarget::DomainHandler::DomainHandler(const std::string& domain)
    : domain_(domain) {
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsBrowserTarget::DomainHandler::HandleCommand(
    DevToolsProtocol::Command* command) {
  CommandHandlers::iterator it = command_handlers_.find(command->method());
  if (it == command_handlers_.end()) {
    return command->NoSuchMethodErrorResponse();
  }
  return (it->second).Run(command);
}

void DevToolsBrowserTarget::DomainHandler::SendNotification(
    const std::string& method,
    base::DictionaryValue* params) {
  notifier_.Run(method, params);
}

DevToolsBrowserTarget::DevToolsBrowserTarget(
    base::MessageLoopProxy* message_loop_proxy,
    net::HttpServer* http_server,
    int connection_id)
    : message_loop_proxy_(message_loop_proxy),
      http_server_(http_server),
      connection_id_(connection_id),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

DevToolsBrowserTarget::~DevToolsBrowserTarget() {
  for (DomainHandlerMap::iterator i = handlers_.begin(); i != handlers_.end();
       ++i)
    delete i->second;
}

void DevToolsBrowserTarget::RegisterDomainHandler(DomainHandler* handler) {
  std::string domain = handler->domain();
  DCHECK(handlers_.find(domain) == handlers_.end());
  handlers_[domain] = handler;
  handler->set_notifier(Bind(&DevToolsBrowserTarget::SendNotification,
                             weak_factory_.GetWeakPtr()));
}

std::string DevToolsBrowserTarget::HandleMessage(const std::string& data) {
  std::string error_response;
  scoped_ptr<DevToolsProtocol::Command> command(
      DevToolsProtocol::ParseCommand(data, &error_response));
  if (!command.get())
    return error_response;

  if (handlers_.find(command->domain()) == handlers_.end()) {
    scoped_ptr<DevToolsProtocol::Response> response(
        command->NoSuchMethodErrorResponse());
    return response->Serialize();
  }

  scoped_ptr<DevToolsProtocol::Response> response(
      handlers_[command->domain()]->HandleCommand(command.get()));

  return response->Serialize();
}

void DevToolsBrowserTarget::SendNotification(const std::string& method,
                                             DictionaryValue* params) {
  DevToolsProtocol::Notification notification(method, params);
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::SendOverWebSocket,
                 http_server_,
                 connection_id_,
                 notification.Serialize()));
}

}  // namespace content

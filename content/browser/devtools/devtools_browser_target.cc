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
#include "net/server/http_server.h"

namespace content {

DevToolsBrowserTarget::DevToolsBrowserTarget(
    base::MessageLoopProxy* message_loop_proxy,
    net::HttpServer* http_server,
    int connection_id)
    : message_loop_proxy_(message_loop_proxy),
      http_server_(http_server),
      connection_id_(connection_id),
      handlers_deleter_(&handlers_),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

DevToolsBrowserTarget::~DevToolsBrowserTarget() {
}

void DevToolsBrowserTarget::RegisterDomainHandler(
    const std::string& domain,
    DevToolsProtocol::Handler* handler) {
  DCHECK(handlers_.find(domain) == handlers_.end());
  handlers_[domain] = handler;
  handler->SetNotifier(base::Bind(&DevToolsBrowserTarget::OnNotification,
                                  weak_factory_.GetWeakPtr()));
}

std::string DevToolsBrowserTarget::HandleMessage(const std::string& data) {
  std::string error_response;
  scoped_ptr<DevToolsProtocol::Command> command(
      DevToolsProtocol::ParseCommand(data, &error_response));
  if (!command)
    return error_response;

  if (handlers_.find(command->domain()) == handlers_.end())
    return command->NoSuchMethodErrorResponse()->Serialize();

  scoped_ptr<DevToolsProtocol::Response> response(
      handlers_[command->domain()]->HandleCommand(command.get()));
  if (!response)
    return command->NoSuchMethodErrorResponse()->Serialize();
  return response->Serialize();
}

void DevToolsBrowserTarget::OnNotification(const std::string& message) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::SendOverWebSocket,
                 http_server_,
                 connection_id_,
                 message));
}

}  // namespace content

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_client.h"

#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/devtools_server.h"

namespace ui {
namespace devtools {

UiDevToolsClient::UiDevToolsClient(const std::string& name,
                                   UiDevToolsServer* server)
    : name_(name),
      connection_id_(kNotConnected),
      dispatcher_(this),
      server_(server) {
  DCHECK(server_);
}

UiDevToolsClient::~UiDevToolsClient() {}

void UiDevToolsClient::AddDOMBackend(
    std::unique_ptr<protocol::DOM::Backend> dom_backend) {
  dom_backend_ = std::move(dom_backend);
  protocol::DOM::Dispatcher::wire(&dispatcher_, dom_backend_.get());
}

void UiDevToolsClient::Dispatch(const std::string& data) {
  dispatcher_.dispatch(protocol::parseJSON(data));
}

bool UiDevToolsClient::connected() const {
  return connection_id_ != kNotConnected;
}

void UiDevToolsClient::set_connection_id(int connection_id) {
  connection_id_ = connection_id;
}

const std::string& UiDevToolsClient::name() const {
  return name_;
}

void UiDevToolsClient::sendProtocolResponse(int callId, const String& message) {
  if (connected())
    server_->SendOverWebSocket(connection_id_, message);
}

void UiDevToolsClient::sendProtocolNotification(const String& message) {
  if (connected())
    server_->SendOverWebSocket(connection_id_, message);
}

void UiDevToolsClient::flushProtocolNotifications() {
  NOTIMPLEMENTED();
}

}  // namespace devtools
}  // namespace ui

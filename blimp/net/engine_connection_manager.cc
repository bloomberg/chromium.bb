// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/engine_connection_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_transport.h"
#include "blimp/net/message_port.h"
#include "net/base/net_errors.h"

namespace blimp {

EngineConnectionManager::EngineConnectionManager(
    ConnectionHandler* connection_handler)
    : connection_handler_(connection_handler) {
  DCHECK(connection_handler_);
}

EngineConnectionManager::~EngineConnectionManager() {}

void EngineConnectionManager::AddTransport(
    std::unique_ptr<BlimpTransport> transport) {
  BlimpTransport* transport_ptr = transport.get();
  transports_.push_back(std::move(transport));
  Connect(transport_ptr);
}

void EngineConnectionManager::Connect(BlimpTransport* transport) {
  transport->Connect(base::Bind(&EngineConnectionManager::OnConnectResult,
                                base::Unretained(this),
                                base::Unretained(transport)));
}

void EngineConnectionManager::OnConnectResult(BlimpTransport* transport,
                                              int result) {
  CHECK_EQ(net::OK, result) << "Transport failure:" << transport->GetName();
  connection_handler_->HandleConnection(
      base::MakeUnique<BlimpConnection>(transport->TakeMessagePort()));
  Connect(transport);
}

}  // namespace blimp

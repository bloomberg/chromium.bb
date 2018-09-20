// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/protocol_connection_server.h"

namespace openscreen {

ProtocolConnectionServer::ProtocolConnectionServer(const ServerConfig& config,
                                                   Observer* observer)
    : config_(config), observer_(observer) {}

ProtocolConnectionServer::~ProtocolConnectionServer() = default;

}  // namespace openscreen

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/protocol_connection_client.h"

namespace openscreen {

ProtocolConnectionClientError::ProtocolConnectionClientError() = default;
ProtocolConnectionClientError::ProtocolConnectionClientError(
    Code error,
    const std::string& message)
  : error(error), message(message) {}
ProtocolConnectionClientError::ProtocolConnectionClientError(
    const ProtocolConnectionClientError& other) =
  default;
ProtocolConnectionClientError::~ProtocolConnectionClientError() = default;

ProtocolConnectionClientError& ProtocolConnectionClientError::operator=(
    const ProtocolConnectionClientError& other) = default;

ProtocolConnectionClient::Metrics::Metrics() = default;
ProtocolConnectionClient::Metrics::~Metrics() = default;

ProtocolConnectionClient::Config::Config() = default;
ProtocolConnectionClient::Config::~Config() = default;

ProtocolConnectionClient::ProtocolConnectionClient(const Config& config,
                                               Observer* observer) :
  config_(config), observer_(observer) {}
ProtocolConnectionClient::~ProtocolConnectionClient() = default;

}  // namespace openscreen

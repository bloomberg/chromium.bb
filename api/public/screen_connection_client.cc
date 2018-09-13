// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/screen_connection_client.h"

namespace openscreen {

ScreenConnectionClientError::ScreenConnectionClientError() = default;
ScreenConnectionClientError::ScreenConnectionClientError(
    Code error,
    const std::string& message)
  : error(error), message(message) {}
ScreenConnectionClientError::ScreenConnectionClientError(
    const ScreenConnectionClientError& other) =
  default;
ScreenConnectionClientError::~ScreenConnectionClientError() = default;

ScreenConnectionClientError& ScreenConnectionClientError::operator=(
    const ScreenConnectionClientError& other) = default;

ScreenConnectionClient::Metrics::Metrics() = default;
ScreenConnectionClient::Metrics::~Metrics() = default;

ScreenConnectionClient::Config::Config() = default;
ScreenConnectionClient::Config::~Config() = default;

ScreenConnectionClient::ScreenConnectionClient(const Config& config,
                                               Observer* observer) :
  config_(config), observer_(observer) {}
ScreenConnectionClient::~ScreenConnectionClient() = default;

}  // namespace openscreen

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/protocol_connection_client.h"

namespace openscreen {

ProtocolConnectionClient::ProtocolConnectionClient(
    ProtocolConnectionObserver* observer)
    : observer_(observer) {}

ProtocolConnectionClient::~ProtocolConnectionClient() = default;

}  // namespace openscreen

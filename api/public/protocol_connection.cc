// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/protocol_connection.h"

#include "platform/api/logging.h"

namespace openscreen {

ProtocolConnection::ProtocolConnection(uint64_t endpoint_id,
                                       uint64_t connection_id)
    : endpoint_id_(endpoint_id), connection_id_(connection_id) {}

void ProtocolConnection::SetObserver(Observer* observer) {
  OSP_DCHECK(!observer_ || !observer);
  observer_ = observer;
}

}  // namespace openscreen

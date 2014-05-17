// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session_base.h"

namespace net {

QuicClientSessionBase::QuicClientSessionBase(
    QuicConnection* connection,
    uint32 max_flow_control_receive_window_bytes,
    const QuicConfig& config)
    : QuicSession(connection,
                  max_flow_control_receive_window_bytes,
                  config) {}

QuicClientSessionBase::~QuicClientSessionBase() {}

}  // namespace net

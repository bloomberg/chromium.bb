// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_test_util.h"

#include <utility>

#include "net/base/ip_address.h"

namespace cast_channel {

MockCastTransport::MockCastTransport() {}
MockCastTransport::~MockCastTransport() {}

CastTransport::Delegate* MockCastTransport::current_delegate() const {
  CHECK(delegate_);
  return delegate_.get();
}

void MockCastTransport::SetReadDelegate(
    std::unique_ptr<CastTransport::Delegate> delegate) {
  delegate_ = std::move(delegate);
}

MockCastTransportDelegate::MockCastTransportDelegate() {}
MockCastTransportDelegate::~MockCastTransportDelegate() {}

MockCastSocketObserver::MockCastSocketObserver() {}
MockCastSocketObserver::~MockCastSocketObserver() {}

MockCastSocket::MockCastSocket() : mock_transport_(new MockCastTransport()) {}
MockCastSocket::~MockCastSocket() {}

net::IPEndPoint CreateIPEndPointForTest() {
  return net::IPEndPoint(net::IPAddress(192, 168, 1, 1), 8009);
}

}  // namespace cast_channel

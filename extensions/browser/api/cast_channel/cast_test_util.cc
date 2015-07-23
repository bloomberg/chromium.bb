// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_test_util.h"

namespace extensions {
namespace api {
namespace cast_channel {

const char kTestExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";

MockCastTransport::MockCastTransport() {
}
MockCastTransport::~MockCastTransport() {
}

CastTransport::Delegate* MockCastTransport::current_delegate() const {
  CHECK(delegate_);
  return delegate_.get();
}

void MockCastTransport::SetReadDelegate(
    scoped_ptr<CastTransport::Delegate> delegate) {
  delegate_ = delegate.Pass();
}

MockCastTransportDelegate::MockCastTransportDelegate() {
}
MockCastTransportDelegate::~MockCastTransportDelegate() {
}

MockCastSocket::MockCastSocket()
    : CastSocket(kTestExtensionId), mock_transport_(new MockCastTransport) {
}
MockCastSocket::~MockCastSocket() {
}

net::IPEndPoint CreateIPEndPointForTest() {
  net::IPAddressNumber number;
  number.push_back(192);
  number.push_back(168);
  number.push_back(1);
  number.push_back(1);
  return net::IPEndPoint(number, 8009);
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/test_util.h"

namespace extensions {
namespace core_api {
namespace cast_channel {

MockCastTransport::MockCastTransport() {
}
MockCastTransport::~MockCastTransport() {
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
}  // namespace core_api
}  // namespace extensions

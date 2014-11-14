// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_transport.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {
namespace core_api {
namespace cast_channel {

class MockCastTransport
    : public extensions::core_api::cast_channel::CastTransport {
 public:
  MockCastTransport();

  virtual ~MockCastTransport() override;

  MOCK_METHOD2(
      SendMessage,
      void(const extensions::core_api::cast_channel::CastMessage& message,
           const net::CompletionCallback& callback));
  MOCK_METHOD0(StartReading, void(void));
  MOCK_METHOD1(SetReadDelegate, void(CastTransport::Delegate* delegate));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastTransport);
};

// Creates the IPEndpoint 192.168.1.1.
net::IPEndPoint CreateIPEndPointForTest();

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

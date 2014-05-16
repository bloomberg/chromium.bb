// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/net_utility.h"

#include "base/basictypes.h"
#include "net/base/net_errors.h"
#include "net/udp/udp_socket.h"

namespace media {
namespace cast {
namespace test {

// TODO(hubbe): Move to /net/.
net::IPEndPoint GetFreeLocalPort() {
  net::IPAddressNumber localhost;
  localhost.push_back(127);
  localhost.push_back(0);
  localhost.push_back(0);
  localhost.push_back(1);
  scoped_ptr<net::UDPSocket> receive_socket(
      new net::UDPSocket(net::DatagramSocket::DEFAULT_BIND,
                         net::RandIntCallback(),
                         NULL,
                         net::NetLog::Source()));
  receive_socket->AllowAddressReuse();
  CHECK_EQ(net::OK, receive_socket->Bind(net::IPEndPoint(localhost, 0)));
  net::IPEndPoint endpoint;
  CHECK_EQ(net::OK, receive_socket->GetLocalAddress(&endpoint));
  return endpoint;
}

}  // namespace test
}  // namespace cast
}  // namespace media

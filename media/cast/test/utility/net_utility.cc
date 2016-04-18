// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/net_utility.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/udp/udp_server_socket.h"

namespace media {
namespace cast {
namespace test {

// TODO(hubbe): Move to /net/.
net::IPEndPoint GetFreeLocalPort() {
  scoped_ptr<net::UDPServerSocket> receive_socket(
      new net::UDPServerSocket(NULL, net::NetLog::Source()));
  receive_socket->AllowAddressReuse();
  CHECK_EQ(net::OK, receive_socket->Listen(
                        net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)));
  net::IPEndPoint endpoint;
  CHECK_EQ(net::OK, receive_socket->GetLocalAddress(&endpoint));
  return endpoint;
}

}  // namespace test
}  // namespace cast
}  // namespace media

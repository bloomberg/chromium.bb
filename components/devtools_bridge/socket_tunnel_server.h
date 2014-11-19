// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_SOCKET_TUNNEL_SERVER_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_SOCKET_TUNNEL_SERVER_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"

namespace net {
class StreamSocket;
}

namespace devtools_bridge {

class AbstractDataChannel;
class SessionDependencyFactory;

class SocketTunnelServer {
 public:
  SocketTunnelServer(SessionDependencyFactory* factory,
                     AbstractDataChannel* data_channel,
                     const std::string& socket_name);
  ~SocketTunnelServer();

 private:
  class Connection;
  class ConnectionController;
  class DataChannelObserver;

  AbstractDataChannel* const data_channel_;

  DISALLOW_COPY_AND_ASSIGN(SocketTunnelServer);
};

}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_SOCKET_TUNNEL_SERVER_H_

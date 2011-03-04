// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_P2P_IPC_SOCKET_FACTORY_H_
#define CHROME_RENDERER_P2P_IPC_SOCKET_FACTORY_H_

#include "base/basictypes.h"
#include "third_party/libjingle/source/talk/base/packetsocketfactory.h"

class P2PSocketDispatcher;

// IpcPacketSocketFactory implements talk_base::PacketSocketFactory
// interface for libjingle using IPC-based P2P sockets. The class must
// be used on a thread that is a libjingle thread (implements
// talk_base::Thread) and also has associated base::MessageLoop. Each
// socket created by the factory must be used on the thread it was
// created on.
class IpcPacketSocketFactory : public talk_base::PacketSocketFactory {
 public:
  explicit IpcPacketSocketFactory(P2PSocketDispatcher* socket_dispatcher);
  virtual ~IpcPacketSocketFactory();

  virtual talk_base::AsyncPacketSocket* CreateUdpSocket(
      const talk_base::SocketAddress& local_address,
      int min_port, int max_port);
  virtual talk_base::AsyncPacketSocket* CreateServerTcpSocket(
      const talk_base::SocketAddress& local_address, int min_port, int max_port,
      bool listen, bool ssl);
  virtual talk_base::AsyncPacketSocket* CreateClientTcpSocket(
      const talk_base::SocketAddress& local_address,
      const talk_base::SocketAddress& remote_address,
      const talk_base::ProxyInfo& proxy_info,
      const std::string& user_agent,
      bool ssl);

 private:
  P2PSocketDispatcher* socket_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(IpcPacketSocketFactory);
};

#endif  // CHROME_RENDERER_P2P_IPC_SOCKET_FACTORY_H_

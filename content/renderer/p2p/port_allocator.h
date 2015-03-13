// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_
#define CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_

#include "third_party/webrtc/p2p/client/basicportallocator.h"
#include "url/gurl.h"

namespace content {

class P2PPortAllocatorSession;
class P2PSocketDispatcher;

class P2PPortAllocator : public cricket::BasicPortAllocator {
 public:
  struct Config {
    Config();
    ~Config();

    struct RelayServerConfig {
      RelayServerConfig();
      ~RelayServerConfig();

      std::string username;
      std::string password;
      std::string server_address;
      int port;
      std::string transport_type;
      bool secure;
    };

    std::set<rtc::SocketAddress> stun_servers;

    std::vector<RelayServerConfig> relays;

    // Disable TCP-based transport when set to true.
    bool disable_tcp_transport;

    // Disable binding to individual NICs when set to false.
    bool enable_multiple_routes;
  };

  P2PPortAllocator(P2PSocketDispatcher* socket_dispatcher,
                   rtc::NetworkManager* network_manager,
                   rtc::PacketSocketFactory* socket_factory,
                   const Config& config,
                   const GURL& origin);
  ~P2PPortAllocator() override;

  cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password) override;

 private:
  friend class P2PPortAllocatorSession;

  P2PSocketDispatcher* socket_dispatcher_;
  Config config_;
  GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(P2PPortAllocator);
};

class P2PPortAllocatorSession : public cricket::BasicPortAllocatorSession {
 public:
  P2PPortAllocatorSession(
      P2PPortAllocator* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password);
  ~P2PPortAllocatorSession() override;

 protected:
  // Overrides for cricket::BasicPortAllocatorSession.
  void GetPortConfigurations() override;

 private:
  P2PPortAllocator* allocator_;

  DISALLOW_COPY_AND_ASSIGN(P2PPortAllocatorSession);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_

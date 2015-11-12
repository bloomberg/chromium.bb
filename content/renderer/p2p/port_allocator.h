// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_
#define CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
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

    // Enable non-proxied UDP-based transport when set to true. When set to
    // false, it effectively disables all UDP traffic until UDP-supporting proxy
    // RETURN is available in the future.
    bool enable_nonproxied_udp = true;

    // Request binding to individual NICs. Whether multiple routes is allowed is
    // subject to the permission check on mic/camera. When specified as false or
    // the permission request is denied, it still uses the default local address
    // to generate a single local candidate. TODO(guoweis): Rename this to
    // |request_multiple_routes|.
    bool enable_multiple_routes = true;

    // Enable exposing the default local address when set to true. This is
    // only in effect when the |enable_multiple_routes| is false or the
    // permission check of mic/camera is denied.
    bool enable_default_local_candidate = true;
  };

  P2PPortAllocator(
      const scoped_refptr<P2PSocketDispatcher>& socket_dispatcher,
      scoped_ptr<rtc::NetworkManager> network_manager,
      rtc::PacketSocketFactory* socket_factory,
      const Config& config,
      const GURL& origin,
      const scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~P2PPortAllocator() override;

  cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password) override;

 private:
  friend class P2PPortAllocatorSession;
  scoped_ptr<rtc::NetworkManager> network_manager_;
  scoped_refptr<P2PSocketDispatcher> socket_dispatcher_;
  Config config_;
  GURL origin_;

  // This is the thread |network_manager_| is associated with and must be used
  // to delete |network_manager_|.
  const scoped_refptr<base::SingleThreadTaskRunner>
      network_manager_task_runner_;

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

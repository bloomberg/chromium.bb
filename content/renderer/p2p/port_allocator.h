// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_
#define CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_

#include "base/memory/ref_counted.h"
#include "net/base/net_util.h"
#include "third_party/libjingle/source/talk/p2p/client/basicportallocator.h"
#include "webkit/glue/p2p_transport.h"

namespace content {

class P2PHostAddressRequest;
class P2PPortAllocatorSession;
class P2PSocketDispatcher;

class P2PPortAllocator : public cricket::BasicPortAllocator{
 public:
  P2PPortAllocator(P2PSocketDispatcher* socket_dispatcher,
                   talk_base::NetworkManager* network_manager,
                   talk_base::PacketSocketFactory* socket_factory,
                   const webkit_glue::P2PTransport::Config& config);
  virtual ~P2PPortAllocator();

  virtual cricket::PortAllocatorSession* CreateSession(
      const std::string& name,
      const std::string& session_type) OVERRIDE;

 private:
  friend class P2PPortAllocatorSession;

  P2PSocketDispatcher* socket_dispatcher_;
  webkit_glue::P2PTransport::Config config_;

  DISALLOW_COPY_AND_ASSIGN(P2PPortAllocator);
};

class P2PPortAllocatorSession : public cricket::BasicPortAllocatorSession {
 public:
  P2PPortAllocatorSession(
      P2PPortAllocator* allocator,
      const std::string& name,
      const std::string& session_type);
  virtual ~P2PPortAllocatorSession();

 protected:
  // Overrides for cricket::BasicPortAllocatorSession.
  virtual void GetPortConfigurations() OVERRIDE;

 private:
  void ResolveStunServerAddress();
  void OnStunServerAddress(const net::IPAddressNumber& address);

  P2PPortAllocator* allocator_;

  scoped_refptr<P2PHostAddressRequest> stun_address_request_;

  DISALLOW_COPY_AND_ASSIGN(P2PPortAllocatorSession);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_

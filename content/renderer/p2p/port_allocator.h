// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_
#define CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_

#include "base/memory/ref_counted.h"
#include "net/base/net_util.h"
#include "third_party/libjingle/source/talk/p2p/client/basicportallocator.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLLoaderClient.h"
#include "webkit/glue/p2p_transport.h"

namespace WebKit {
class WebFrame;
class WebURLLoader;
}  // namespace WebKit

namespace content {

class P2PHostAddressRequest;
class P2PPortAllocatorSession;
class P2PSocketDispatcher;

// TODO(sergeyu): There is overlap between this class and
// HttpPortAllocator. Refactor HttpPortAllocator
class P2PPortAllocator : public cricket::BasicPortAllocator {
 public:
  P2PPortAllocator(WebKit::WebFrame* web_frame,
                   P2PSocketDispatcher* socket_dispatcher,
                   talk_base::NetworkManager* network_manager,
                   talk_base::PacketSocketFactory* socket_factory,
                   const webkit_glue::P2PTransport::Config& config);
  virtual ~P2PPortAllocator();

  virtual cricket::PortAllocatorSession* CreateSession(
      const std::string& name,
      const std::string& session_type) OVERRIDE;

 private:
  friend class P2PPortAllocatorSession;

  WebKit::WebFrame* web_frame_;
  P2PSocketDispatcher* socket_dispatcher_;
  webkit_glue::P2PTransport::Config config_;

  DISALLOW_COPY_AND_ASSIGN(P2PPortAllocator);
};

class P2PPortAllocatorSession : public cricket::BasicPortAllocatorSession,
                                public WebKit::WebURLLoaderClient  {
 public:
  P2PPortAllocatorSession(
      P2PPortAllocator* allocator,
      const std::string& name,
      const std::string& session_type);
  virtual ~P2PPortAllocatorSession();

  // WebKit::WebURLLoaderClient overrides.
  virtual void didReceiveData(WebKit::WebURLLoader* loader,
                              const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE;
  virtual void didFinishLoading(WebKit::WebURLLoader* loader,
                                double finish_time) OVERRIDE;
  virtual void didFail(WebKit::WebURLLoader* loader,
                       const WebKit::WebURLError& error) OVERRIDE;

 protected:
  // Overrides for cricket::BasicPortAllocatorSession.
  virtual void GetPortConfigurations() OVERRIDE;

 private:
  void ResolveStunServerAddress();
  void OnStunServerAddress(const net::IPAddressNumber& address);

  void AllocateRelaySession();
  void ParseRelayResponse();

  void AddConfig();

  P2PPortAllocator* allocator_;

  scoped_refptr<P2PHostAddressRequest> stun_address_request_;
  talk_base::SocketAddress stun_server_address_;

  scoped_ptr<WebKit::WebURLLoader> relay_session_request_;
  int relay_session_attempts_;
  std::string relay_session_response_;
  std::string relay_username_;
  std::string relay_password_;
  talk_base::SocketAddress relay_ip_;
  int relay_udp_port_;
  int relay_tcp_port_;
  int relay_ssltcp_port_;

  DISALLOW_COPY_AND_ASSIGN(P2PPortAllocatorSession);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_PORT_ALLOCATOR_H_

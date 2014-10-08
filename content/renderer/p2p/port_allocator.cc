// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/port_allocator.h"

namespace content {

P2PPortAllocator::Config::Config()
    : disable_tcp_transport(false) {
}

P2PPortAllocator::Config::~Config() {
}

P2PPortAllocator::Config::RelayServerConfig::RelayServerConfig()
    : port(0) {
}

P2PPortAllocator::Config::RelayServerConfig::~RelayServerConfig() {
}

P2PPortAllocator::P2PPortAllocator(
    P2PSocketDispatcher* socket_dispatcher,
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory,
    const Config& config)
    : cricket::BasicPortAllocator(network_manager, socket_factory),
      socket_dispatcher_(socket_dispatcher),
      config_(config) {
  uint32 flags = 0;
  if (config_.disable_tcp_transport)
    flags |= cricket::PORTALLOCATOR_DISABLE_TCP;
  set_flags(flags);
  set_allow_tcp_listen(false);
}

P2PPortAllocator::~P2PPortAllocator() {
}

cricket::PortAllocatorSession* P2PPortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password) {
  return new P2PPortAllocatorSession(
      this, content_name, component, ice_username_fragment, ice_password);
}

P2PPortAllocatorSession::P2PPortAllocatorSession(
    P2PPortAllocator* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password)
    : cricket::BasicPortAllocatorSession(allocator,
                                         content_name,
                                         component,
                                         ice_username_fragment,
                                         ice_password),
      allocator_(allocator) {
}

P2PPortAllocatorSession::~P2PPortAllocatorSession() {
}

void P2PPortAllocatorSession::GetPortConfigurations() {
  const P2PPortAllocator::Config& config = allocator_->config_;
  cricket::PortConfiguration* port_config = new cricket::PortConfiguration(
      config.stun_servers, std::string(), std::string());

  for (size_t i = 0; i < config.relays.size(); ++i) {
    cricket::RelayCredentials credentials(config.relays[i].username,
                                          config.relays[i].password);
    cricket::RelayServerConfig relay_server(cricket::RELAY_TURN);
    cricket::ProtocolType protocol;
    if (!cricket::StringToProto(config.relays[i].transport_type.c_str(),
                                &protocol)) {
      DLOG(WARNING) << "Ignoring TURN server "
                    << config.relays[i].server_address << ". "
                    << "Reason= Incorrect "
                    << config.relays[i].transport_type
                    << " transport parameter.";
      continue;
    }

    relay_server.ports.push_back(cricket::ProtocolAddress(
        rtc::SocketAddress(config.relays[i].server_address,
                                 config.relays[i].port),
        protocol,
        config.relays[i].secure));
    relay_server.credentials = credentials;
    port_config->AddRelay(relay_server);
  }
  ConfigReady(port_config);
}

}  // namespace content

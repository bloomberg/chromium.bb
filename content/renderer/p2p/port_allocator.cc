// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/port_allocator.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/p2p/socket_dispatcher.h"

namespace content {

P2PPortAllocator::Config::Config() {}

P2PPortAllocator::Config::~Config() {
}

P2PPortAllocator::Config::RelayServerConfig::RelayServerConfig()
    : port(0) {
}

P2PPortAllocator::Config::RelayServerConfig::~RelayServerConfig() {
}

P2PPortAllocator::P2PPortAllocator(
    const scoped_refptr<P2PSocketDispatcher>& socket_dispatcher,
    scoped_ptr<rtc::NetworkManager> network_manager,
    rtc::PacketSocketFactory* socket_factory,
    const Config& config,
    const GURL& origin,
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : cricket::BasicPortAllocator(network_manager.get(), socket_factory),
      network_manager_(network_manager.Pass()),
      socket_dispatcher_(socket_dispatcher),
      config_(config),
      origin_(origin),
      network_manager_task_runner_(task_runner) {
  uint32 flags = 0;
  if (!config_.enable_multiple_routes)
    flags |= cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION;
  if (!config_.enable_default_local_candidate)
    flags |= cricket::PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE;
  if (!config_.enable_nonproxied_udp) {
    flags |= cricket::PORTALLOCATOR_DISABLE_UDP |
             cricket::PORTALLOCATOR_DISABLE_STUN |
             cricket::PORTALLOCATOR_DISABLE_UDP_RELAY;
  }
  set_flags(flags);
  set_allow_tcp_listen(false);
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  bool enable_webrtc_stun_origin =
      cmd_line->HasSwitch(switches::kEnableWebRtcStunOrigin);
  if (enable_webrtc_stun_origin) {
    set_origin(origin_.spec());
  }
}

// TODO(guoweis): P2PPortAllocator is also deleted in the wrong thread
// here. It's created in signaling thread, and held by webrtc::PeerConnection,
// only used on worker thread. The destructor is invoked on signaling thread
// again. crbug.com/535761. DeleteSoon can be removed once the bug is fixed.
P2PPortAllocator::~P2PPortAllocator() {
  network_manager_task_runner_->DeleteSoon(FROM_HERE,
                                           network_manager_.release());
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

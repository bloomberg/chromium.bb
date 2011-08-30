// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/port_allocator.h"

#include "base/bind.h"
#include "content/renderer/p2p/host_address_request.h"
#include "jingle/glue/utils.h"
#include "net/base/ip_endpoint.h"

namespace content {

P2PPortAllocator::P2PPortAllocator(
    P2PSocketDispatcher* socket_dispatcher,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    const webkit_glue::P2PTransport::Config& config)
    : cricket::BasicPortAllocator(network_manager, socket_factory),
      socket_dispatcher_(socket_dispatcher),
      config_(config) {
}

P2PPortAllocator::~P2PPortAllocator() {
}

cricket::PortAllocatorSession* P2PPortAllocator::CreateSession(
    const std::string& name,
    const std::string& session_type) {
  return new P2PPortAllocatorSession(this, name, session_type);
}

P2PPortAllocatorSession::P2PPortAllocatorSession(
    P2PPortAllocator* allocator,
    const std::string& name,
    const std::string& session_type)
    : cricket::BasicPortAllocatorSession(allocator, name, session_type),
      allocator_(allocator) {
}

P2PPortAllocatorSession::~P2PPortAllocatorSession() {
  if (stun_address_request_)
    stun_address_request_->Cancel();
}

void P2PPortAllocatorSession::GetPortConfigurations() {
  // Add am empty configuration synchronously, so a local connection
  // can be started immediately.
  ConfigReady(new cricket::PortConfiguration(
      talk_base::SocketAddress(), "", "", ""));

  ResolveStunServerAddress();

  // TODO(sergeyu): Implement relay server support.
}

void P2PPortAllocatorSession::ResolveStunServerAddress() {
 if (allocator_->config_.stun_server.empty())
   return;

 DCHECK(!stun_address_request_);
 stun_address_request_ =
     new P2PHostAddressRequest(allocator_->socket_dispatcher_);
 stun_address_request_->Request(allocator_->config_.stun_server, base::Bind(
     &P2PPortAllocatorSession::OnStunServerAddress,
     base::Unretained(this)));
}

void P2PPortAllocatorSession::OnStunServerAddress(
    const net::IPAddressNumber& address) {
  if (address.empty()) {
    LOG(ERROR) << "Failed to resolve STUN server address "
               << allocator_->config_.stun_server;
    return;
  }

  talk_base::SocketAddress socket_address;
  if (!jingle_glue::IPEndPointToSocketAddress(
          net::IPEndPoint(address, allocator_->config_.stun_server_port),
          &socket_address)) {
    return;
  }

  ConfigReady(new cricket::PortConfiguration(socket_address, "", "", ""));
}

}  // namespace content

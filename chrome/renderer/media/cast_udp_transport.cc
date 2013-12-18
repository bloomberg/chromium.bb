// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_udp_transport.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/renderer/media/cast_session.h"
#include "content/public/renderer/p2p_socket_client.h"
#include "content/public/renderer/p2p_socket_client_delegate.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"

CastUdpTransport::CastUdpTransport(
    const scoped_refptr<CastSession>& session)
    : cast_session_(session), weak_factory_(this) {
}

CastUdpTransport::~CastUdpTransport() {
  if (socket_)
    socket_->Close();
}

void CastUdpTransport::Start(const net::IPEndPoint& remote_address) {
  remote_address_ = remote_address;
  // TODO(hclam): This code binds to a fixed port for now. P2P socket will be
  // deprecated once we move packet sending to the browser and this code
  // should be removed.
  socket_ = content::P2PSocketClient::Create(
      content::P2P_SOCKET_UDP,
      net::IPEndPoint(net::IPAddressNumber(4, 0), 2346),
      remote_address,
      this);
  CHECK(socket_);

  cast_session_->StartSending(
      base::Bind(&CastUdpTransport::SendPacket,
                 weak_factory_.GetWeakPtr()));
}

void CastUdpTransport::OnOpen(
    const net::IPEndPoint& address) {
  // Called once Init completes. Ignored.
}

void CastUdpTransport::OnIncomingTcpConnection(
    const net::IPEndPoint& address,
    content::P2PSocketClient* client) {
  // We only support UDP sockets. This function should not be called
  // for UDP sockets.
  NOTREACHED();
}

void CastUdpTransport::OnSendComplete() {
  // Ignored for now.
}

void CastUdpTransport::OnError() {
  NOTIMPLEMENTED();
}

void CastUdpTransport::OnDataReceived(const net::IPEndPoint& address,
                                      const std::vector<char>& data,
                                      const base::TimeTicks& timestamp) {
  cast_session_->ReceivePacket(data);
}

void CastUdpTransport::SendPacket(const std::vector<char>& packet) {
  socket_->SendWithDscp(remote_address_, packet, net::DSCP_AF41);
}

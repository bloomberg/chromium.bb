// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_udp_transport.h"

#include "base/logging.h"
#include "chrome/renderer/media/cast_session.h"
#include "content/public/renderer/p2p_socket_client.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"

class CastUdpSocketFactory : public CastSession::P2PSocketFactory {
 public:
  virtual scoped_refptr<content::P2PSocketClient> Create() OVERRIDE {
    net::IPEndPoint unspecified_end_point;
    scoped_refptr<content::P2PSocketClient> socket =
        content::P2PSocketClient::Create(
            content::P2P_SOCKET_UDP,
            unspecified_end_point,
            unspecified_end_point,
            NULL);
    return socket;
  }
};

CastUdpTransport::CastUdpTransport(
    const scoped_refptr<CastSession>& session)
    : cast_session_(session) {
}

CastUdpTransport::~CastUdpTransport() {
}

void CastUdpTransport::Start(const net::IPEndPoint& remote_address) {
  cast_session_->SetSocketFactory(
      scoped_ptr<CastSession::P2PSocketFactory>(
          new CastUdpSocketFactory()).Pass(),
      remote_address);
}

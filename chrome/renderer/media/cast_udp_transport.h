// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_
#define CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/p2p_socket_client.h"
#include "content/public/renderer/p2p_socket_client_delegate.h"
#include "net/base/host_port_pair.h"

class CastSession;

// This class represents the transport mechanism used by Cast RTP streams
// to connect to a remote client. It specifies the destination address
// and network protocol used to send Cast RTP streams.
class CastUdpTransport : public content::P2PSocketClientDelegate {
 public:
  explicit CastUdpTransport(const scoped_refptr<CastSession>& session);
  virtual ~CastUdpTransport();

  // Begin the transport by specifying the remote IP address.
  // The transport will use UDP.
  void Start(const net::IPEndPoint& remote_address);

 protected:
  // content::P2PSocketClient::Delegate Implementation
  virtual void OnOpen(const net::IPEndPoint& address) OVERRIDE;
  virtual void OnIncomingTcpConnection(
      const net::IPEndPoint& address,
      content::P2PSocketClient* client) OVERRIDE;
  virtual void OnSendComplete() OVERRIDE;
  virtual void OnError() OVERRIDE;
  virtual void OnDataReceived(const net::IPEndPoint& address,
                              const std::vector<char>& data,
                              const base::TimeTicks& timestamp) OVERRIDE;

 private:
  void SendPacket(const std::vector<char>& packet);

  const scoped_refptr<CastSession> cast_session_;
  scoped_refptr<content::P2PSocketClient> socket_;
  net::IPEndPoint remote_address_;
  base::WeakPtrFactory<CastUdpTransport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastUdpTransport);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_
#define CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "net/base/host_port_pair.h"

class CastSession;

// This class represents an end point to which communication is done by
// UDP. The interface does not allow direct access to a UDP socket but
// represents a transport mechanism.
//
// CastUdpTransport creates a CastSession and then shares it with
// multiple CastSendTransports. This is because CastSession corresponds
// to only one remote peer.
class CastUdpTransport {
 public:
  CastUdpTransport();
  ~CastUdpTransport();

  // Begin the transport by specifying the remote IP address.
  // The transport will use UDP.
  void Start(const net::HostPortPair& remote_address);

  // Terminate the communication with the end point.
  void Stop();

  scoped_refptr<CastSession> cast_session() const {
    return cast_session_;
  }

 private:
  const scoped_refptr<CastSession> cast_session_;

  DISALLOW_COPY_AND_ASSIGN(CastUdpTransport);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_

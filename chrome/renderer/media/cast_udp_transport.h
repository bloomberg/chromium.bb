// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_
#define CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "net/base/host_port_pair.h"

class CastSession;

// This class represents the transport mechanism used by Cast RTP streams
// to connect to a remote client. It specifies the destination address
// and network protocol used to send Cast RTP streams.
class CastUdpTransport {
 public:
  explicit CastUdpTransport(const scoped_refptr<CastSession>& session);
  ~CastUdpTransport();

  // Begin the transport by specifying the remote IP address.
  // The transport will use UDP.
  void Start(const net::IPEndPoint& remote_address);

 private:
  const scoped_refptr<CastSession> cast_session_;

  DISALLOW_COPY_AND_ASSIGN(CastUdpTransport);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_

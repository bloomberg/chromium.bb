// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_
#define CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_

#include "base/basictypes.h"
#include "net/base/ip_endpoint.h"

// This class represents an end point to which communication is done by
// UDP. The interface does not allow direct access to a UDP socket but
// represents a transport mechanism.
class CastUdpTransport {
 public:
  CastUdpTransport();
  ~CastUdpTransport();

  // Begin the transport by specifying the remote IP address.
  // The transport will use UDP.
  void Start(const net::IPEndPoint& remote_address);

  // Terminate the communication with the end point.
  void Stop();

 private:
  DISALLOW_COPY_AND_ASSIGN(CastUdpTransport);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_NEXT_PROTO_H_
#define NET_SOCKET_NEXT_PROTO_H_

namespace net {

// Next Protocol Negotiation (NPN), if successful, results in agreement on an
// application-level string that specifies the application level protocol to
// use over the TLS connection. NextProto enumerates the application level
// protocols that we recognise.
enum NextProto {
  kProtoUnknown = 0,
  kProtoHTTP11 = 1,
  kProtoMinimumVersion = kProtoHTTP11,

  // TODO(akalin): Stop advertising SPDY/1 and remove this.
  kProtoSPDY1 = 2,
  kProtoSPDYMinimumVersion = kProtoSPDY1,
  kProtoSPDY2 = 3,
  // TODO(akalin): Stop adverising SPDY/2.1, too.
  kProtoSPDY21 = 4,
  kProtoSPDY3 = 5,
  kProtoSPDY31 = 6,
  kProtoSPDY4a2 = 7,
  // We lump in HTTP/2 with the SPDY protocols for now.
  kProtoHTTP2Draft04 = 8,
  kProtoSPDYMaximumVersion = kProtoHTTP2Draft04,

  kProtoQUIC1SPDY3 = 9,

  kProtoMaximumVersion = kProtoQUIC1SPDY3,
};

}  // namespace net

#endif  // NET_SOCKET_NEXT_PROTO_H_

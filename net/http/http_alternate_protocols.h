// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpAlternateProtocols is an in-memory data structure used for keeping track
// of which HTTP HostPortPairs have an alternate protocol that can be used
// instead of HTTP on a different port.

#ifndef NET_HTTP_HTTP_ALTERNATE_PROTOCOLS_H_
#define NET_HTTP_HTTP_ALTERNATE_PROTOCOLS_H_
#pragma once

#include <map>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "net/base/host_port_pair.h"

namespace net {

class HttpAlternateProtocols {
 public:
  enum Protocol {
    NPN_SPDY_1,
    NPN_SPDY_2,
    NUM_ALTERNATE_PROTOCOLS,
    BROKEN,  // The alternate protocol is known to be broken.
    UNINITIALIZED,
  };

  struct PortProtocolPair {
    bool Equals(const PortProtocolPair& other) const {
      return port == other.port && protocol == other.protocol;
    }

    std::string ToString() const;

    uint16 port;
    Protocol protocol;
  };

  typedef std::map<HostPortPair, PortProtocolPair> ProtocolMap;

  static const char kHeader[];
  static const char* const kProtocolStrings[NUM_ALTERNATE_PROTOCOLS];

  HttpAlternateProtocols();
  ~HttpAlternateProtocols();

  // Reports whether or not we have received Alternate-Protocol for
  // |http_host_port_pair|.
  bool HasAlternateProtocolFor(const HostPortPair& http_host_port_pair) const;
  bool HasAlternateProtocolFor(const std::string& host, uint16 port) const;

  PortProtocolPair GetAlternateProtocolFor(
      const HostPortPair& http_host_port_pair) const;
  PortProtocolPair GetAlternateProtocolFor(
      const std::string& host, uint16 port) const;

  // SetAlternateProtocolFor() will ignore the request if the alternate protocol
  // has already been marked broken via MarkBrokenAlternateProtocolFor().
  void SetAlternateProtocolFor(const HostPortPair& http_host_port_pair,
                               uint16 alternate_port,
                               Protocol alternate_protocol);

  // Marks the alternate protocol as broken.  Once marked broken, any further
  // attempts to set the alternate protocol for |http_host_port_pair| will fail.
  void MarkBrokenAlternateProtocolFor(const HostPortPair& http_host_port_pair);

  const ProtocolMap& protocol_map() const { return protocol_map_; }

  // Debugging to simulate presence of an AlternateProtocol.
  // If we don't have an alternate protocol in the map for any given host/port
  // pair, force this ProtocolPortPair.
  static void ForceAlternateProtocol(const PortProtocolPair& pair);
  static void DisableForcedAlternateProtocol();

 private:
  ProtocolMap protocol_map_;

  static const char* ProtocolToString(Protocol protocol);

  // The forced alternate protocol.  If not-null, there is a protocol being
  // forced.
  static PortProtocolPair* forced_alternate_protocol_;

  DISALLOW_COPY_AND_ASSIGN(HttpAlternateProtocols);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_ALTERNATE_PROTOCOLS_H_

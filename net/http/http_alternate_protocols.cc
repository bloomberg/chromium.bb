// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_alternate_protocols.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"

namespace net {

const char HttpAlternateProtocols::kHeader[] = "Alternate-Protocol";
const char* const HttpAlternateProtocols::kProtocolStrings[] = {
  "npn-spdy/1",
};

HttpAlternateProtocols::HttpAlternateProtocols() {}
HttpAlternateProtocols::~HttpAlternateProtocols() {}

bool HttpAlternateProtocols::HasAlternateProtocolFor(
    const HostPortPair& http_host_port_pair) const {
  return ContainsKey(protocol_map_, http_host_port_pair);
}

bool HttpAlternateProtocols::HasAlternateProtocolFor(
    const std::string& host, uint16 port) const {
  struct HostPortPair http_host_port_pair;
  http_host_port_pair.host = host;
  http_host_port_pair.port = port;
  return HasAlternateProtocolFor(http_host_port_pair);
}

HttpAlternateProtocols::PortProtocolPair
HttpAlternateProtocols::GetAlternateProtocolFor(
    const HostPortPair& http_host_port_pair) const {
  DCHECK(ContainsKey(protocol_map_, http_host_port_pair));
  return protocol_map_.find(http_host_port_pair)->second;
}

HttpAlternateProtocols::PortProtocolPair
HttpAlternateProtocols::GetAlternateProtocolFor(
    const std::string& host, uint16 port) const {
  struct HostPortPair http_host_port_pair;
  http_host_port_pair.host = host;
  http_host_port_pair.port = port;
  return GetAlternateProtocolFor(http_host_port_pair);
}

void HttpAlternateProtocols::SetAlternateProtocolFor(
    const HostPortPair& http_host_port_pair,
    uint16 alternate_port,
    Protocol alternate_protocol) {
  if (alternate_protocol == BROKEN) {
    LOG(DFATAL) << "Call MarkBrokenAlternateProtocolFor() instead.";
    return;
  }

  PortProtocolPair alternate;
  alternate.port = alternate_port;
  alternate.protocol = alternate_protocol;
  if (HasAlternateProtocolFor(http_host_port_pair)) {
    const PortProtocolPair existing_alternate =
        GetAlternateProtocolFor(http_host_port_pair);

    if (existing_alternate.protocol == BROKEN) {
      DLOG(INFO) << "Ignore alternate protocol since it's known to be broken.";
      return;
    }

    if (alternate_protocol != BROKEN && !existing_alternate.Equals(alternate)) {
      LOG(WARNING) << "Changing the alternate protocol for: "
                   << http_host_port_pair.ToString()
                   << " from [Port: " << existing_alternate.port
                   << ", Protocol: " << existing_alternate.protocol
                   << "] to [Port: " << alternate_port
                   << ", Protocol: " << alternate_protocol
                   << "].";
    }
  }

  protocol_map_[http_host_port_pair] = alternate;
}

void HttpAlternateProtocols::MarkBrokenAlternateProtocolFor(
    const HostPortPair& http_host_port_pair) {
  protocol_map_[http_host_port_pair].protocol = BROKEN;
}

}  // namespace net

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_alternate_protocols.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/stl_util-inl.h"

namespace net {

const char HttpAlternateProtocols::kHeader[] = "Alternate-Protocol";
const char* const HttpAlternateProtocols::kProtocolStrings[] = {
  "npn-spdy/1",
  "npn-spdy/2",
};

const char* HttpAlternateProtocols::ProtocolToString(
    HttpAlternateProtocols::Protocol protocol) {
  switch (protocol) {
    case HttpAlternateProtocols::NPN_SPDY_1:
    case HttpAlternateProtocols::NPN_SPDY_2:
      return HttpAlternateProtocols::kProtocolStrings[protocol];
    case HttpAlternateProtocols::BROKEN:
      return "Broken";
    case HttpAlternateProtocols::UNINITIALIZED:
      return "Uninitialized";
    default:
      NOTREACHED();
      return "";
  }
}


std::string HttpAlternateProtocols::PortProtocolPair::ToString() const {
  return base::StringPrintf("%d:%s", port,
                            HttpAlternateProtocols::ProtocolToString(protocol));
}

// static
HttpAlternateProtocols::PortProtocolPair*
    HttpAlternateProtocols::forced_alternate_protocol_ = NULL;

HttpAlternateProtocols::HttpAlternateProtocols() {}
HttpAlternateProtocols::~HttpAlternateProtocols() {}

bool HttpAlternateProtocols::HasAlternateProtocolFor(
    const HostPortPair& http_host_port_pair) const {
  return ContainsKey(protocol_map_, http_host_port_pair) ||
      forced_alternate_protocol_;
}

bool HttpAlternateProtocols::HasAlternateProtocolFor(
    const std::string& host, uint16 port) const {
  HostPortPair http_host_port_pair(host, port);
  return HasAlternateProtocolFor(http_host_port_pair);
}

HttpAlternateProtocols::PortProtocolPair
HttpAlternateProtocols::GetAlternateProtocolFor(
    const HostPortPair& http_host_port_pair) const {
  DCHECK(HasAlternateProtocolFor(http_host_port_pair));

  // First check the map.
  ProtocolMap::const_iterator it = protocol_map_.find(http_host_port_pair);
  if (it != protocol_map_.end())
    return it->second;

  // We must be forcing an alternate.
  DCHECK(forced_alternate_protocol_);
  return *forced_alternate_protocol_;
}

HttpAlternateProtocols::PortProtocolPair
HttpAlternateProtocols::GetAlternateProtocolFor(
    const std::string& host, uint16 port) const {
  HostPortPair http_host_port_pair(host, port);
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
      DVLOG(1) << "Ignore alternate protocol since it's known to be broken.";
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

// static
void HttpAlternateProtocols::ForceAlternateProtocol(
    const PortProtocolPair& pair) {
  // Note: we're going to leak this.
  if (forced_alternate_protocol_)
    delete forced_alternate_protocol_;
  forced_alternate_protocol_ = new PortProtocolPair(pair);
}

// static
void HttpAlternateProtocols::DisableForcedAlternateProtocol() {
  delete forced_alternate_protocol_;
  forced_alternate_protocol_ = NULL;
}

}  // namespace net

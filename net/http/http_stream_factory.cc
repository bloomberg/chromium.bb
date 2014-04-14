// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "url/gurl.h"

namespace net {

// WARNING: If you modify or add any static flags, you must keep them in sync
// with |ResetStaticSettingsToInit|. This is critical for unit test isolation.

// static
std::vector<std::string>* HttpStreamFactory::next_protos_ = NULL;
// static
bool HttpStreamFactory::enabled_protocols_[NUM_VALID_ALTERNATE_PROTOCOLS];
// static
bool HttpStreamFactory::spdy_enabled_ = true;
// static
bool HttpStreamFactory::use_alternate_protocols_ = false;
// static
bool HttpStreamFactory::force_spdy_over_ssl_ = true;
// static
bool HttpStreamFactory::force_spdy_always_ = false;
// static
std::list<HostPortPair>* HttpStreamFactory::forced_spdy_exclusions_ = NULL;

HttpStreamFactory::~HttpStreamFactory() {}

// static
bool HttpStreamFactory::IsProtocolEnabled(AlternateProtocol protocol) {
  DCHECK(IsAlternateProtocolValid(protocol));
  return enabled_protocols_[
      protocol - ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION];
}

// static
void HttpStreamFactory::SetProtocolEnabled(AlternateProtocol protocol) {
  DCHECK(IsAlternateProtocolValid(protocol));
  enabled_protocols_[
      protocol - ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION] = true;
}

// static
void HttpStreamFactory::ResetEnabledProtocols() {
  for (int i = ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION;
       i <= ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION; ++i) {
    enabled_protocols_[i - ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION] = false;
  }
}

// static
void HttpStreamFactory::ResetStaticSettingsToInit() {
  // WARNING: These must match the initializers above.
  delete next_protos_;
  delete forced_spdy_exclusions_;
  next_protos_ = NULL;
  spdy_enabled_ = true;
  use_alternate_protocols_ = false;
  force_spdy_over_ssl_ = true;
  force_spdy_always_ = false;
  forced_spdy_exclusions_ = NULL;
  ResetEnabledProtocols();
}

void HttpStreamFactory::ProcessAlternateProtocol(
    const base::WeakPtr<HttpServerProperties>& http_server_properties,
    const std::string& alternate_protocol_str,
    const HostPortPair& http_host_port_pair) {
  std::vector<std::string> port_protocol_vector;
  base::SplitString(alternate_protocol_str, ':', &port_protocol_vector);
  if (port_protocol_vector.size() != 2) {
    DVLOG(1) << kAlternateProtocolHeader
             << " header has too many tokens: "
             << alternate_protocol_str;
    return;
  }

  int port;
  if (!base::StringToInt(port_protocol_vector[0], &port) ||
      port <= 0 || port >= 1 << 16) {
    DVLOG(1) << kAlternateProtocolHeader
             << " header has unrecognizable port: "
             << port_protocol_vector[0];
    return;
  }

  AlternateProtocol protocol =
      AlternateProtocolFromString(port_protocol_vector[1]);
  if (IsAlternateProtocolValid(protocol) && !IsProtocolEnabled(protocol)) {
    protocol = ALTERNATE_PROTOCOL_BROKEN;
  }

  if (protocol == ALTERNATE_PROTOCOL_BROKEN) {
    DVLOG(1) << kAlternateProtocolHeader
             << " header has unrecognized protocol: "
             << port_protocol_vector[1];
    return;
  }

  HostPortPair host_port(http_host_port_pair);
  const HostMappingRules* mapping_rules = GetHostMappingRules();
  if (mapping_rules)
    mapping_rules->RewriteHost(&host_port);

  if (http_server_properties->HasAlternateProtocol(host_port)) {
    const PortAlternateProtocolPair existing_alternate =
        http_server_properties->GetAlternateProtocol(host_port);
    // If we think the alternate protocol is broken, don't change it.
    if (existing_alternate.protocol == ALTERNATE_PROTOCOL_BROKEN)
      return;
  }

  http_server_properties->SetAlternateProtocol(host_port, port, protocol);
}

GURL HttpStreamFactory::ApplyHostMappingRules(const GURL& url,
                                              HostPortPair* endpoint) {
  const HostMappingRules* mapping_rules = GetHostMappingRules();
  if (mapping_rules && mapping_rules->RewriteHost(endpoint)) {
    url_canon::Replacements<char> replacements;
    const std::string port_str = base::IntToString(endpoint->port());
    replacements.SetPort(port_str.c_str(),
                         url_parse::Component(0, port_str.size()));
    replacements.SetHost(endpoint->host().c_str(),
                         url_parse::Component(0, endpoint->host().size()));
    return url.ReplaceComponents(replacements);
  }
  return url;
}

// static
void HttpStreamFactory::add_forced_spdy_exclusion(const std::string& value) {
  HostPortPair pair = HostPortPair::FromURL(GURL(value));
  if (!forced_spdy_exclusions_)
    forced_spdy_exclusions_ = new std::list<HostPortPair>();
  forced_spdy_exclusions_->push_back(pair);
}

// static
bool HttpStreamFactory::HasSpdyExclusion(const HostPortPair& endpoint) {
  std::list<HostPortPair>* exclusions = forced_spdy_exclusions_;
  if (!exclusions)
    return false;

  std::list<HostPortPair>::const_iterator it;
  for (it = exclusions->begin(); it != exclusions->end(); ++it)
    if (it->Equals(endpoint))
      return true;
  return false;
}

// static
void HttpStreamFactory::EnableNpnHttpOnly() {
  // Avoid alternate protocol in this case. Otherwise, browser will try SSL
  // and then fallback to http. This introduces extra load.
  set_use_alternate_protocols(false);
  std::vector<NextProto> next_protos;
  next_protos.push_back(kProtoHTTP11);
  SetNextProtos(next_protos);
}

// static
void HttpStreamFactory::EnableNpnSpdy3() {
  set_use_alternate_protocols(true);
  std::vector<NextProto> next_protos;
  next_protos.push_back(kProtoHTTP11);
  next_protos.push_back(kProtoQUIC1SPDY3);
  next_protos.push_back(kProtoSPDY3);
  SetNextProtos(next_protos);
}

// static
void HttpStreamFactory::EnableNpnSpdy31() {
  set_use_alternate_protocols(true);
  std::vector<NextProto> next_protos;
  next_protos.push_back(kProtoHTTP11);
  next_protos.push_back(kProtoQUIC1SPDY3);
  next_protos.push_back(kProtoSPDY3);
  next_protos.push_back(kProtoSPDY31);
  SetNextProtos(next_protos);
}

// static
void HttpStreamFactory::EnableNpnSpdy31WithSpdy2() {
  set_use_alternate_protocols(true);
  std::vector<NextProto> next_protos;
  next_protos.push_back(kProtoHTTP11);
  next_protos.push_back(kProtoQUIC1SPDY3);
  next_protos.push_back(kProtoDeprecatedSPDY2);
  next_protos.push_back(kProtoSPDY3);
  next_protos.push_back(kProtoSPDY31);
  SetNextProtos(next_protos);
}

// static
void HttpStreamFactory::EnableNpnSpdy4Http2() {
  set_use_alternate_protocols(true);
  std::vector<NextProto> next_protos;
  next_protos.push_back(kProtoHTTP11);
  next_protos.push_back(kProtoQUIC1SPDY3);
  next_protos.push_back(kProtoSPDY3);
  next_protos.push_back(kProtoSPDY31);
  next_protos.push_back(kProtoSPDY4);
  SetNextProtos(next_protos);
}

// static
void HttpStreamFactory::SetNextProtos(const std::vector<NextProto>& value) {
  if (!next_protos_)
    next_protos_ = new std::vector<std::string>;

  next_protos_->clear();

  ResetEnabledProtocols();

  // TODO(rtenneti): bug 116575 - consider combining the NextProto and
  // AlternateProtocol.
  for (uint32 i = 0; i < value.size(); ++i) {
    NextProto proto = value[i];
    // Add the protocol to the TLS next protocol list, except for QUIC
    // since it uses UDP.
    if (proto != kProtoQUIC1SPDY3) {
      next_protos_->push_back(SSLClientSocket::NextProtoToString(proto));
    }

    // Enable the corresponding alternate protocol, except for HTTP
    // which has not corresponding alternative.
    if (proto != kProtoHTTP11) {
      AlternateProtocol alternate = AlternateProtocolFromNextProto(proto);
      if (!IsAlternateProtocolValid(alternate)) {
        NOTREACHED() << "Invalid next proto: " << proto;
        continue;
      }
      SetProtocolEnabled(alternate);
    }
  }
}

HttpStreamFactory::HttpStreamFactory() {}

}  // namespace net

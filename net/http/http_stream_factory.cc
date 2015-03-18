// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_network_session.h"
#include "url/gurl.h"

namespace net {

// WARNING: If you modify or add any static flags, you must keep them in sync
// with |ResetStaticSettingsToInit|. This is critical for unit test isolation.

// static
bool HttpStreamFactory::spdy_enabled_ = true;

HttpStreamFactory::~HttpStreamFactory() {}

// static
void HttpStreamFactory::ResetStaticSettingsToInit() {
  spdy_enabled_ = true;
}

void HttpStreamFactory::ProcessAlternateProtocol(
    const base::WeakPtr<HttpServerProperties>& http_server_properties,
    const std::vector<std::string>& alternate_protocol_values,
    const HostPortPair& http_host_port_pair,
    const HttpNetworkSession& session) {
  AlternateProtocol protocol = UNINITIALIZED_ALTERNATE_PROTOCOL;
  int port = 0;
  double probability = 1;
  bool is_valid = true;
  for (size_t i = 0; i < alternate_protocol_values.size(); ++i) {
    const std::string& alternate_protocol_str = alternate_protocol_values[i];
    if (StartsWithASCII(alternate_protocol_str, "p=", true)) {
      if (!base::StringToDouble(alternate_protocol_str.substr(2),
                                &probability) ||
          probability < 0 || probability > 1) {
        DVLOG(1) << kAlternateProtocolHeader
                 << " header has unrecognizable probability: "
                 << alternate_protocol_values[i];
        is_valid = false;
        break;
      }
      continue;
    }

    std::vector<std::string> port_protocol_vector;
    base::SplitString(alternate_protocol_str, ':', &port_protocol_vector);
    if (port_protocol_vector.size() != 2) {
      DVLOG(1) << kAlternateProtocolHeader
               << " header has too many tokens: "
               << alternate_protocol_str;
      is_valid = false;
      break;
    }

    if (!base::StringToInt(port_protocol_vector[0], &port) ||
        port == 0 || !IsPortValid(port)) {
      DVLOG(1) << kAlternateProtocolHeader
               << " header has unrecognizable port: "
               << port_protocol_vector[0];
      is_valid = false;
      break;
    }

    protocol = AlternateProtocolFromString(port_protocol_vector[1]);

    if (IsAlternateProtocolValid(protocol) &&
        !session.IsProtocolEnabled(protocol)) {
      DVLOG(1) << kAlternateProtocolHeader
               << " header has unrecognized protocol: "
               << port_protocol_vector[1];
      is_valid = false;
      break;
    }
  }

  if (!is_valid || protocol == UNINITIALIZED_ALTERNATE_PROTOCOL) {
    http_server_properties->ClearAlternateProtocol(http_host_port_pair);
    return;
  }

  HostPortPair host_port(http_host_port_pair);
  const HostMappingRules* mapping_rules = GetHostMappingRules();
  if (mapping_rules)
    mapping_rules->RewriteHost(&host_port);

  http_server_properties->SetAlternateProtocol(
      host_port, static_cast<uint16>(port), protocol, probability);
}

GURL HttpStreamFactory::ApplyHostMappingRules(const GURL& url,
                                              HostPortPair* endpoint) {
  const HostMappingRules* mapping_rules = GetHostMappingRules();
  if (mapping_rules && mapping_rules->RewriteHost(endpoint)) {
    url::Replacements<char> replacements;
    const std::string port_str = base::IntToString(endpoint->port());
    replacements.SetPort(port_str.c_str(), url::Component(0, port_str.size()));
    replacements.SetHost(endpoint->host().c_str(),
                         url::Component(0, endpoint->host().size()));
    return url.ReplaceComponents(replacements);
  }
  return url;
}

HttpStreamFactory::HttpStreamFactory() {}

}  // namespace net

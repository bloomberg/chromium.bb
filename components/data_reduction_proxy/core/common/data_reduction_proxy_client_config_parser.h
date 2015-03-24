// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CLIENT_CONFIG_RESPONSE_PARSER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CLIENT_CONFIG_RESPONSE_PARSER_H_

#include <string>

#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/proxy/proxy_server.h"

namespace base {
class Time;
}

namespace data_reduction_proxy {

namespace config_parser {

// Returns a string representation (which is actually the string representation
// of ProxyServer_ProxyScheme) of a |net::ProxyServer::Scheme|.
std::string GetSchemeString(net::ProxyServer::Scheme scheme);

// Returns the |net::ProxyServer::Scheme| for a ProxyServer_ProxyScheme.
net::ProxyServer::Scheme SchemeFromProxyScheme(
    ProxyServer_ProxyScheme proxy_scheme);

// Retrieves the ProxyServer_ProxyScheme for its string representation.
ProxyServer_ProxyScheme GetProxyScheme(const std::string& scheme);

// Returns the ISO-8601 representation of |time|.
std::string TimeToISO8601(const base::Time& time);

// Parses an ISO-8601 time string into a Timestamp proto.
bool ISO8601ToTimestamp(const std::string& time, Timestamp* timestamp);

// Returns the |base::Time| representation of |timestamp|.
base::Time TimestampToTime(const Timestamp& timestamp);

// Takes a JSON representation of a |ClientConfig| and populates |config|.
// Returns false if the JSON has an unexpected structure.
// TODO(jeremyim): This should be deprecated once gRPC support can be added
// (which would give the binary proto instead of JSON).
bool ParseClientConfig(const std::string& config_data, ClientConfig* config);

}  // namespace config_parser

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CLIENT_CONFIG_RESPONSE_PARSER_H_

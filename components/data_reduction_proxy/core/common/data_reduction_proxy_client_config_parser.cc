// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"

namespace {

// String representations of ProxyServer schemes.
const char kSchemeHTTP[] = "HTTP";
const char kSchemeHTTPS[] = "HTTPS";
const char kSchemeQUIC[] = "QUIC";
const char kSchemeUnspecified[] = "UNSPECIFIED";

}  // namespace

namespace data_reduction_proxy {

namespace config_parser {

std::string GetSchemeString(net::ProxyServer::Scheme scheme) {
  switch (scheme) {
    case net::ProxyServer::SCHEME_HTTP:
      return kSchemeHTTP;
    case net::ProxyServer::SCHEME_HTTPS:
      return kSchemeHTTPS;
    case net::ProxyServer::SCHEME_QUIC:
      return kSchemeQUIC;
    default:
      return kSchemeUnspecified;
  }
}

net::ProxyServer::Scheme SchemeFromProxyScheme(
    ProxyServer_ProxyScheme proxy_scheme) {
  switch (proxy_scheme) {
    case ProxyServer_ProxyScheme_HTTP:
      return net::ProxyServer::SCHEME_HTTP;
    case ProxyServer_ProxyScheme_HTTPS:
      return net::ProxyServer::SCHEME_HTTPS;
    case ProxyServer_ProxyScheme_QUIC:
      return net::ProxyServer::SCHEME_QUIC;
    default:
      return net::ProxyServer::SCHEME_INVALID;
  }
}

ProxyServer_ProxyScheme GetProxyScheme(const std::string& scheme) {
  if (scheme == kSchemeHTTP)
    return ProxyServer_ProxyScheme_HTTP;

  if (scheme == kSchemeHTTPS)
    return ProxyServer_ProxyScheme_HTTPS;

  if (scheme == kSchemeQUIC)
    return ProxyServer_ProxyScheme_QUIC;

  return ProxyServer_ProxyScheme_UNSPECIFIED;
}

std::string TimeToISO8601(const base::Time& time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second,
      exploded.millisecond);
}

bool ISO8601ToTimestamp(const std::string& time, Timestamp* timestamp) {
  base::Time t;
  if (!base::Time::FromUTCString(time.c_str(), &t))
    return false;

  timestamp->set_seconds((t - base::Time::UnixEpoch()).InSeconds());
  // Discard fractional seconds; it isn't worth the code effort to
  // calculate it.
  timestamp->set_nanos(0);
  return true;
}

base::Time TimestampToTime(const Timestamp& timestamp) {
  base::Time t = base::Time::UnixEpoch();
  t += base::TimeDelta::FromSeconds(timestamp.seconds());
  t += base::TimeDelta::FromMicroseconds(
      timestamp.nanos() / base::Time::kNanosecondsPerMicrosecond);
  return t;
}

bool ParseClientConfig(const std::string& config_data, ClientConfig* config) {
  scoped_ptr<base::Value> parsed_data = base::JSONReader::Read(config_data);
  if (!parsed_data)
    return false;

  const base::DictionaryValue* parsed_dict;
  if (!parsed_data->GetAsDictionary(&parsed_dict))
    return false;

  std::string session_key;
  if (!parsed_dict->GetString("sessionKey", &session_key))
    return false;

  config->set_session_key(session_key);

  std::string expire_time;
  if (!parsed_dict->GetString("expireTime", &expire_time))
    return false;

  if (!ISO8601ToTimestamp(expire_time, config->mutable_expire_time()))
    return false;

  const base::DictionaryValue* proxy_config_dict;
  if (!parsed_dict->GetDictionary("proxyConfig", &proxy_config_dict))
    return false;

  ProxyConfig* proxy_config = config->mutable_proxy_config();

  const base::ListValue* http_proxy_servers;
  if (!proxy_config_dict->GetList("httpProxyServers", &http_proxy_servers))
    return false;

  base::ListValue::const_iterator it = http_proxy_servers->begin();
  for (; it != http_proxy_servers->end(); ++it) {
    const base::DictionaryValue* server_value;
    if (!(*it)->GetAsDictionary(&server_value)) {
      continue;
    }

    std::string scheme;
    std::string host;
    int port;
    if (!server_value->GetString("scheme", &scheme)) {
      continue;
    }

    if (!server_value->GetString("host", &host)) {
      continue;
    }

    if (!server_value->GetInteger("port", &port)) {
      continue;
    }

    ProxyServer_ProxyScheme proxy_scheme = GetProxyScheme(scheme);
    if (proxy_scheme == ProxyServer_ProxyScheme_UNSPECIFIED)
      continue;

    ProxyServer* proxy_server = proxy_config->add_http_proxy_servers();
    proxy_server->set_scheme(GetProxyScheme(scheme));
    proxy_server->set_host(host);
    proxy_server->set_port(port);
  }

  return true;
}

}  // namespace config_parser

}  // namespace data_reduction_proxy

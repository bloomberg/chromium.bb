// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"

#include "base/time/time.h"

namespace data_reduction_proxy {

namespace config_parser {

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

ProxyServer_ProxyScheme ProxySchemeFromScheme(net::ProxyServer::Scheme scheme) {
  switch (scheme) {
    case net::ProxyServer::SCHEME_HTTP:
      return ProxyServer_ProxyScheme_HTTP;
    case net::ProxyServer::SCHEME_HTTPS:
      return ProxyServer_ProxyScheme_HTTPS;
    case net::ProxyServer::SCHEME_QUIC:
      return ProxyServer_ProxyScheme_QUIC;
    default:
      return ProxyServer_ProxyScheme_UNSPECIFIED;
  }
}

void TimetoTimestamp(const base::Time& time, Timestamp* timestamp) {
  timestamp->set_seconds((time - base::Time::UnixEpoch()).InSeconds());
  // Discard fractional seconds; it isn't worth the code effort to
  // calculate it.
  timestamp->set_nanos(0);
}

void TimeDeltatoDuration(const base::TimeDelta& time_delta,
                         Duration* duration) {
  duration->set_seconds(time_delta.InSeconds());
  // Discard fractional seconds; it isn't worth the code effort to
  // calculate it.
  duration->set_nanos(0);
}

base::Time TimestampToTime(const Timestamp& timestamp) {
  base::Time t = base::Time::UnixEpoch();
  t += base::TimeDelta::FromSeconds(timestamp.seconds());
  t += base::TimeDelta::FromMicroseconds(
      timestamp.nanos() / base::Time::kNanosecondsPerMicrosecond);
  return t;
}

base::TimeDelta DurationToTimeDelta(const Duration& duration) {
  return base::TimeDelta::FromSeconds(duration.seconds()) +
         base::TimeDelta::FromMicroseconds(
             duration.nanos() / base::Time::kNanosecondsPerMicrosecond);
}

}  // namespace config_parser

}  // namespace data_reduction_proxy

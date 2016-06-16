// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_UTIL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_UTIL_H_

#include <memory>
#include <string>

#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"

namespace base {
class Time;
class TimeDelta;
}

namespace net {
class ProxyConfig;
class ProxyInfo;
}

namespace data_reduction_proxy {

// TODO(ryansturm): Move these methods to util namespace. crbug.com/620161

// Returns true if the request method is idempotent.
bool IsMethodIdempotent(const std::string& method);

GURL AddApiKeyToUrl(const GURL& url);

// Returns whether this is valid for data reduction proxy use. |proxy_info|
// should contain a single DIRECT ProxyServer, |url| should not be WS or WSO,
// and the |method| should be idempotent for this to be eligible.
bool EligibleForDataReductionProxy(const net::ProxyInfo& proxy_info,
                                   const GURL& url,
                                   const std::string& method);

// Determines if |proxy_config| would override a direct. |proxy_config| should
// be a data reduction proxy config with proxy servers mapped in the rules.
// |proxy_retry_info| contains the list of bad proxies. |url| is used to
// determine whether it is HTTP or HTTPS. |data_reduction_proxy_info| is an out
// param that will contain the proxies that should be used.
bool ApplyProxyConfigToProxyInfo(const net::ProxyConfig& proxy_config,
                                 const net::ProxyRetryInfoMap& proxy_retry_info,
                                 const GURL& url,
                                 net::ProxyInfo* data_reduction_proxy_info);

namespace protobuf_parser {

// Returns the |net::ProxyServer::Scheme| for a ProxyServer_ProxyScheme.
net::ProxyServer::Scheme SchemeFromProxyScheme(
    ProxyServer_ProxyScheme proxy_scheme);

// Returns the ProxyServer_ProxyScheme for a |net::ProxyServer::Scheme|.
ProxyServer_ProxyScheme ProxySchemeFromScheme(net::ProxyServer::Scheme scheme);

// Returns the |Duration| representation of |time_delta|.
void TimeDeltaToDuration(const base::TimeDelta& time_delta, Duration* duration);

// Returns the |base::TimeDelta| representation of |duration|.  This is accurate
// to the microsecond.
base::TimeDelta DurationToTimeDelta(const Duration& duration);

// Returns the |Timestamp| representation of |time|.
void TimeToTimestamp(const base::Time& time, Timestamp* timestamp);

// Returns the |Time| representation of |timestamp|. This is accurate to the
// microsecond.
base::Time TimestampToTime(const Timestamp& timestamp);

// Returns an allocated |Duration| unique pointer.
std::unique_ptr<Duration> CreateDurationFromTimeDelta(
    const base::TimeDelta& time_delta);

// Returns an allocated |Timestamp| unique pointer.
std::unique_ptr<Timestamp> CreateTimestampFromTime(const base::Time& time);

}  // namespace protobuf_parser

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_UTIL_H_

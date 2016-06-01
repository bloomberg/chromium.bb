// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_UTIL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_UTIL_H_

#include <memory>
#include <string>

#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"

namespace base {
class Time;
class TimeDelta;
}

namespace data_reduction_proxy {

// Returns true if the request method is idempotent.
bool IsMethodIdempotent(const std::string& method);

GURL AddApiKeyToUrl(const GURL& url);

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

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_PROXY_FALLBACK_H_
#define NET_HTTP_PROXY_FALLBACK_H_

// ------------------------------------------------------------
// Proxy Fallback Overview
// ------------------------------------------------------------
//
// Proxy fallback is a feature that is split between the proxy resolution layer
// and the HTTP layers.
//
// The proxy resolution layer is responsible for:
//   * Obtaining a list of proxies to use
//     (ProxyResolutionService::ResolveProxy). Proxy lists are (usually) the
//     result of having evaluated a PAC script, such as:
//         return "PROXY foobar1:8080; HTTPS foobar2:8080; DIRECT";
//
//   * Re-ordering the proxy list such that proxies that have recently failed
//     are given lower priority (ProxyInfo::DeprioritizeBadProxies)
//
//   * Maintaining the expiring cache of proxies that have recently failed.
//
//
// The HTTP layer is responsible for:
//   * Attempting to issue the URLRequest through each of the
//     proxies, in the order specified by the list.
//
//   * Deciding whether this attempt was successful, whether it was a failure
//     but should keep trying other proxies, or whether it was a failure and
//     should stop trying other proxies.
//
//   * Upon successful completion of an attempt though a proxy, calling
//     ProxyResolutionService::ReportSuccess to inform it of all the failed
//     attempts that were made. (A proxy is only considered to be "bad"
//     if the request was able to be completed through some other proxy).
//
//
// Exactly how to interpret the proxy lists returned by PAC is not specified by
// a standard. The justifications for what errors are considered for fallback
// are given beside the implementation.

#include "net/base/net_export.h"

namespace net {

// Returns true if the request should be re-tried using the next proxy in the
// fallback list.
//
// |*error| is the network error that the request failed with. When returning
// false it may be replaced with a different error.
NET_EXPORT bool CanFalloverToNextProxy(int* error);

}  // namespace net

#endif  // NET_HTTP_PROXY_FALLBACK_H_

// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_with_annotation.h"

namespace net {

namespace {

// TODO(rhalavati): Update annotation.
constexpr NetworkTrafficAnnotationTag kDirectProxyTrafficAnnotation =
    DefineNetworkTrafficAnnotation("proxy_config_direct", R"(
    semantics {
      sender: "Proxy Config"
      description:
        "This settings create a direct proxy, which practically connects "
        "directly to the target website without any intervention."
      trigger:
        "Direct proxy is the default mode of connection."
      data:
        "None."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: NO
      setting:
        "Selecting a proxy in settings will prevent use of direct proxy."
      policy_exception_justification:
        "Using either of 'ProxyMode', 'ProxyServer', or 'ProxyPacUrl' policies "
        "can set Chrome to use a specific proxy settings and avoid directly "
        "connecting to the websites."
    })");

}  // namespace

ProxyConfigWithAnnotation::ProxyConfigWithAnnotation()
    : value_(ProxyConfig::CreateDirect()),
      traffic_annotation_(
          MutableNetworkTrafficAnnotationTag(kDirectProxyTrafficAnnotation)) {}

ProxyConfigWithAnnotation::ProxyConfigWithAnnotation(
    const ProxyConfig& proxy_config,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : value_(proxy_config),
      traffic_annotation_(
          MutableNetworkTrafficAnnotationTag(traffic_annotation)) {}

}  // namespace net
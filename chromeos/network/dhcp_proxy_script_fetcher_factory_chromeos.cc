// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/dhcp_proxy_script_fetcher_factory_chromeos.h"

#include <memory>

#include "chromeos/network/dhcp_proxy_script_fetcher_chromeos.h"

namespace chromeos {

DhcpProxyScriptFetcherFactoryChromeos::DhcpProxyScriptFetcherFactoryChromeos() {
}

DhcpProxyScriptFetcherFactoryChromeos::
    ~DhcpProxyScriptFetcherFactoryChromeos() {}

std::unique_ptr<net::DhcpProxyScriptFetcher>
DhcpProxyScriptFetcherFactoryChromeos::Create(
    net::URLRequestContext* url_request_context) {
  return std::make_unique<DhcpProxyScriptFetcherChromeos>(url_request_context);
}

}  // namespace chromeos

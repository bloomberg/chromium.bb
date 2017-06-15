// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_DHCP_PROXY_SCRIPT_FETCHER_FACTORY_CHROMEOS_H_
#define CHROMEOS_NETWORK_DHCP_PROXY_SCRIPT_FETCHER_FACTORY_CHROMEOS_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "net/proxy/dhcp_proxy_script_fetcher_factory.h"

namespace net {
class DhcpProxyScriptFetcher;
class URLRequestContext;
}

namespace chromeos {

// ChromeOS specific implementation of DhcpProxyScriptFetcherFactory.
// TODO(mmenke):  This won't work at all with an out-of-process network service.
// Figure out a way forward there.
class CHROMEOS_EXPORT DhcpProxyScriptFetcherFactoryChromeos
    : public net::DhcpProxyScriptFetcherFactory {
 public:
  DhcpProxyScriptFetcherFactoryChromeos();
  ~DhcpProxyScriptFetcherFactoryChromeos() override;

  // net::DhcpProxyScriptFetcherFactory implementation.
  std::unique_ptr<net::DhcpProxyScriptFetcher> Create(
      net::URLRequestContext* url_request_context) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DhcpProxyScriptFetcherFactoryChromeos);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_DHCP_PROXY_SCRIPT_FETCHER_FACTORY_CHROMEOS_H_

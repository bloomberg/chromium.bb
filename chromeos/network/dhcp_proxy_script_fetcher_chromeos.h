// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_DHCP_PROXY_SCRIPT_FETCHER_CHROMEOS_H_
#define CHROMEOS_NETWORK_DHCP_PROXY_SCRIPT_FETCHER_CHROMEOS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "net/proxy/dhcp_proxy_script_fetcher.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class URLRequestContext;
class ProxyScriptFetcher;
}

namespace chromeos {

// ChromeOS specific implementation of DhcpProxyScriptFetcher.
// This looks up Service.WebProxyAutoDiscoveryUrl for the default network
// from Shill and uses that to fetch the proxy script if available.
class CHROMEOS_EXPORT DhcpProxyScriptFetcherChromeos
    : public net::DhcpProxyScriptFetcher {
 public:
  explicit DhcpProxyScriptFetcherChromeos(
      net::URLRequestContext* url_request_context);
  ~DhcpProxyScriptFetcherChromeos() override;

  // net::DhcpProxyScriptFetcher
  int Fetch(base::string16* utf16_text,
            const net::CompletionCallback& callback) override;
  void Cancel() override;
  const GURL& GetPacURL() const override;
  std::string GetFetcherName() const override;

 private:
  void ContinueFetch(base::string16* utf16_text,
                     net::CompletionCallback callback,
                     std::string pac_url);

  net::URLRequestContext* url_request_context_;  // Weak ptr
  scoped_ptr<net::ProxyScriptFetcher> proxy_script_fetcher_;
  scoped_refptr<base::SingleThreadTaskRunner> network_handler_task_runner_;

  GURL pac_url_;

  base::WeakPtrFactory<DhcpProxyScriptFetcherChromeos> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DhcpProxyScriptFetcherChromeos);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_DHCP_PROXY_SCRIPT_FETCHER_CHROMEOS_H_

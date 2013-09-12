// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_DHCP_PROXY_SCRIPT_FETCHER_CHROMEOS_H_
#define CHROMEOS_NETWORK_DHCP_PROXY_SCRIPT_FETCHER_CHROMEOS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chromeos/chromeos_export.h"
#include "net/proxy/dhcp_proxy_script_fetcher.h"
#include "url/gurl.h"

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
  virtual ~DhcpProxyScriptFetcherChromeos() OVERRIDE;

  // net::DhcpProxyScriptFetcher
  virtual int Fetch(base::string16* utf16_text,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual const GURL& GetPacURL() const OVERRIDE;
  virtual std::string GetFetcherName() const OVERRIDE;

 private:
  void ContinueFetch(base::string16* utf16_text,
                     net::CompletionCallback callback,
                     std::string pac_url);

  net::URLRequestContext* url_request_context_;  // Weak ptr
  scoped_ptr<net::ProxyScriptFetcher> proxy_script_fetcher_;
  scoped_refptr<base::MessageLoopProxy> network_handler_message_loop_;
  base::WeakPtrFactory<DhcpProxyScriptFetcherChromeos> weak_ptr_factory_;

  GURL pac_url_;

  DISALLOW_COPY_AND_ASSIGN(DhcpProxyScriptFetcherChromeos);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_DHCP_PROXY_SCRIPT_FETCHER_CHROMEOS_H_

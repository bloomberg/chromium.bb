// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_IMPL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_IMPL_H_

#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"
#include "chrome/common/local_discovery/service_discovery_client.h"

namespace local_discovery {

class PrivetHTTPAsynchronousFactoryImpl : public PrivetHTTPAsynchronousFactory {
 public:
  PrivetHTTPAsynchronousFactoryImpl(
      ServiceDiscoveryClient* service_discovery_client,
      net::URLRequestContextGetter* request_context);
  virtual ~PrivetHTTPAsynchronousFactoryImpl();

  virtual scoped_ptr<PrivetHTTPResolution> CreatePrivetHTTP(
      const std::string& name,
      const net::HostPortPair& address,
      const ResultCallback& callback) OVERRIDE;

 private:
  class ResolutionImpl : public PrivetHTTPResolution {
   public:
    ResolutionImpl(const std::string& name,
                   const net::HostPortPair& address,
                   const ResultCallback& callback,
                   ServiceDiscoveryClient* service_discovery_client,
                   net::URLRequestContextGetter* request_context);
    virtual ~ResolutionImpl();

    virtual void Start() OVERRIDE;
    virtual const std::string& GetName() OVERRIDE;

   private:
    void ResolveComplete(bool success,
                         const net::IPAddressNumber& address_ipv4,
                         const net::IPAddressNumber& address_ipv6);

    std::string name_;
    scoped_ptr<LocalDomainResolver> resolver_;
    net::HostPortPair hostport_;
    ResultCallback callback_;
    scoped_refptr<net::URLRequestContextGetter> request_context_;
  };

  ServiceDiscoveryClient* service_discovery_client_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_IMPL_H_

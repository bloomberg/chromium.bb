// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_IMPL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_IMPL_H_

#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"
#include "chrome/common/local_discovery/service_discovery_client.h"

namespace local_discovery {

class ServiceDiscoverySharedClient;

class PrivetHTTPAsynchronousFactoryImpl : public PrivetHTTPAsynchronousFactory {
 public:
  explicit PrivetHTTPAsynchronousFactoryImpl(
      net::URLRequestContextGetter* request_context);
  ~PrivetHTTPAsynchronousFactoryImpl() override;

  scoped_ptr<PrivetHTTPResolution> CreatePrivetHTTP(
      const std::string& service_name) override;

 private:
  class ResolutionImpl : public PrivetHTTPResolution {
   public:
    ResolutionImpl(const std::string& service_name,
                   net::URLRequestContextGetter* request_context);
    ~ResolutionImpl() override;

    void Start(const ResultCallback& callback) override;

    void Start(const net::HostPortPair& address,
               const ResultCallback& callback) override;

    const std::string& GetName() override;

   private:
    void ServiceResolveComplete(const ResultCallback& callback,
                                ServiceResolver::RequestStatus result,
                                const ServiceDescription& description);

    void DomainResolveComplete(uint16 port,
                               const ResultCallback& callback,
                               bool success,
                               const net::IPAddressNumber& address_ipv4,
                               const net::IPAddressNumber& address_ipv6);

    std::string name_;
    scoped_refptr<net::URLRequestContextGetter> request_context_;
    scoped_refptr<ServiceDiscoverySharedClient> service_discovery_client_;
    scoped_ptr<ServiceResolver> service_resolver_;
    scoped_ptr<LocalDomainResolver> domain_resolver_;
  };

  scoped_refptr<net::URLRequestContextGetter> request_context_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_IMPL_H_

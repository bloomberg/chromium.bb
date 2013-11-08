// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/browser/local_discovery/privet_http_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/local_discovery/service_discovery_client.h"

namespace local_discovery {

namespace {

std::string IPAddressToHostString(const net::IPAddressNumber& address) {
  std::string address_str = net::IPAddressToString(address);

  // IPv6 addresses need to be surrounded by brackets.
  if (address.size() == net::kIPv6AddressSize) {
    address_str = base::StringPrintf("[%s]", address_str.c_str());
  }

  return address_str;
}

}  // namespace

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

PrivetHTTPAsynchronousFactoryImpl::PrivetHTTPAsynchronousFactoryImpl(
    ServiceDiscoveryClient* service_discovery_client,
    net::URLRequestContextGetter* request_context)
    : service_discovery_client_(service_discovery_client),
      request_context_(request_context) {
}

PrivetHTTPAsynchronousFactoryImpl::~PrivetHTTPAsynchronousFactoryImpl() {
}

// static
scoped_ptr<PrivetHTTPAsynchronousFactory>
    PrivetHTTPAsynchronousFactory::CreateInstance(
        ServiceDiscoveryClient* service_discovery_client,
        net::URLRequestContextGetter* request_context) {
  return make_scoped_ptr<PrivetHTTPAsynchronousFactory>(
      new PrivetHTTPAsynchronousFactoryImpl(service_discovery_client,
                                            request_context));
}

scoped_ptr<PrivetHTTPResolution>
    PrivetHTTPAsynchronousFactoryImpl::CreatePrivetHTTP(
        const std::string& name,
        const net::HostPortPair& address,
        const ResultCallback& callback) {
  return scoped_ptr<PrivetHTTPResolution>(
      new ResolutionImpl(name, address, callback, service_discovery_client_,
                         request_context_.get()));
}

PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::ResolutionImpl(
    const std::string& name,
    const net::HostPortPair& address,
    const ResultCallback& callback,
    ServiceDiscoveryClient* service_discovery_client,
    net::URLRequestContextGetter* request_context)
    : name_(name), hostport_(address), callback_(callback),
      request_context_(request_context) {
  net::AddressFamily address_family = net::ADDRESS_FAMILY_UNSPECIFIED;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPrivetIPv6Only)) {
    address_family = net::ADDRESS_FAMILY_IPV6;
  }

  resolver_ = service_discovery_client->CreateLocalDomainResolver(
      address.host(), address_family,
      base::Bind(&ResolutionImpl::ResolveComplete, base::Unretained(this)));
}

PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::~ResolutionImpl() {
}

void PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::Start() {
  resolver_->Start();
}

const std::string&
PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::GetName() {
  return name_;
}

void PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::ResolveComplete(
    bool success,
    const net::IPAddressNumber& address_ipv4,
    const net::IPAddressNumber& address_ipv6) {
  if (!success) {
    callback_.Run(scoped_ptr<PrivetHTTPClient>());
    return;
  }

  net::IPAddressNumber address = address_ipv4;
  if (address.empty())
    address = address_ipv6;

  DCHECK(!address.empty());

  net::HostPortPair new_address = net::HostPortPair(
      IPAddressToHostString(address), hostport_.port());
  callback_.Run(scoped_ptr<PrivetHTTPClient>(
      new PrivetHTTPClientImpl(name_, new_address, request_context_.get())));
}

}  // namespace local_discovery

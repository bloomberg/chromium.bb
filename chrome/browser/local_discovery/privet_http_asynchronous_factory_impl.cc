// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_http_asynchronous_factory_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/local_discovery/privet_http_impl.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/net_util.h"

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

PrivetHTTPAsynchronousFactoryImpl::PrivetHTTPAsynchronousFactoryImpl(
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context) {
}

PrivetHTTPAsynchronousFactoryImpl::~PrivetHTTPAsynchronousFactoryImpl() {
}

scoped_ptr<PrivetHTTPResolution>
PrivetHTTPAsynchronousFactoryImpl::CreatePrivetHTTP(
    const std::string& service_name) {
  return scoped_ptr<PrivetHTTPResolution>(
      new ResolutionImpl(service_name, request_context_.get()));
}

PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::ResolutionImpl(
    const std::string& service_name,
    net::URLRequestContextGetter* request_context)
    : name_(service_name), request_context_(request_context) {
  service_discovery_client_ = ServiceDiscoverySharedClient::GetInstance();
}

PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::~ResolutionImpl() {
}

const std::string&
PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::GetName() {
  return name_;
}

void PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::Start(
    const ResultCallback& callback) {
  service_resolver_ = service_discovery_client_->CreateServiceResolver(
      name_, base::Bind(&ResolutionImpl::ServiceResolveComplete,
                        base::Unretained(this), callback));
  service_resolver_->StartResolving();
}

void PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::ServiceResolveComplete(
    const ResultCallback& callback,
    ServiceResolver::RequestStatus result,
    const ServiceDescription& description) {
  if (result != ServiceResolver::STATUS_SUCCESS)
    return callback.Run(scoped_ptr<PrivetHTTPClient>());

  Start(description.address, callback);
}

void PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::Start(
    const net::HostPortPair& address,
    const ResultCallback& callback) {
#if defined(OS_MACOSX)
  net::IPAddressNumber ip_address;
  DCHECK(net::ParseIPLiteralToNumber(address.host(), &ip_address));
  // MAC already has IP there.
  callback.Run(scoped_ptr<PrivetHTTPClient>(
      new PrivetHTTPClientImpl(name_, address, request_context_.get())));
#else  // OS_MACOSX
  net::AddressFamily address_family = net::ADDRESS_FAMILY_UNSPECIFIED;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPrivetIPv6Only)) {
    address_family = net::ADDRESS_FAMILY_IPV6;
  }

  domain_resolver_ = service_discovery_client_->CreateLocalDomainResolver(
      address.host(), address_family,
      base::Bind(&ResolutionImpl::DomainResolveComplete, base::Unretained(this),
                 address.port(), callback));
  domain_resolver_->Start();
#endif  // OS_MACOSX
}

void PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::DomainResolveComplete(
    uint16 port,
    const ResultCallback& callback,
    bool success,
    const net::IPAddressNumber& address_ipv4,
    const net::IPAddressNumber& address_ipv6) {
  if (!success) {
    callback.Run(scoped_ptr<PrivetHTTPClient>());
    return;
  }

  net::IPAddressNumber address = address_ipv4;
  if (address.empty())
    address = address_ipv6;

  DCHECK(!address.empty());

  net::HostPortPair new_address =
      net::HostPortPair(IPAddressToHostString(address), port);
  callback.Run(scoped_ptr<PrivetHTTPClient>(
      new PrivetHTTPClientImpl(name_, new_address, request_context_.get())));
}

}  // namespace local_discovery

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_http_asynchronous_factory_mac.h"

#include "chrome/browser/local_discovery/privet_http_impl.h"

namespace local_discovery {

PrivetHTTPAsynchronousFactoryMac::PrivetHTTPAsynchronousFactoryMac(
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context) {
}

PrivetHTTPAsynchronousFactoryMac::~PrivetHTTPAsynchronousFactoryMac() {
}

scoped_ptr<PrivetHTTPResolution>
PrivetHTTPAsynchronousFactoryMac::CreatePrivetHTTP(
    const std::string& name,
    const net::HostPortPair& address,
    const ResultCallback& callback) {
  return scoped_ptr<PrivetHTTPResolution>(
      new ResolutionMac(request_context_, name, address, callback));
}

PrivetHTTPAsynchronousFactoryMac::ResolutionMac::ResolutionMac(
    net::URLRequestContextGetter* request_context,
    const std::string& name,
    const net::HostPortPair& host_port,
    const ResultCallback& callback)
    : request_context_(request_context),
      name_(name),
      host_port_(host_port),
      callback_(callback) {
}

PrivetHTTPAsynchronousFactoryMac::ResolutionMac::~ResolutionMac() {
}

void PrivetHTTPAsynchronousFactoryMac::ResolutionMac::Start() {
  callback_.Run(scoped_ptr<PrivetHTTPClient>(
      new PrivetHTTPClientImpl(name_, host_port_, request_context_)));
}

const std::string& PrivetHTTPAsynchronousFactoryMac::ResolutionMac::GetName() {
  return name_;
}

}  // namespace local_discovery

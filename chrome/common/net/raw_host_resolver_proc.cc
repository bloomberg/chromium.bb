// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/raw_host_resolver_proc.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace chrome_common_net {

RawHostResolverProc::RawHostResolverProc(const net::IPAddressNumber& dns_server,
                                         net::HostResolverProc* previous)
    : HostResolverProc(previous), dns_server_(dns_server) {}

int RawHostResolverProc::Resolve(const std::string& host,
                                 net::AddressFamily address_family,
                                 net::HostResolverFlags host_resolver_flags,
                                 net::AddressList* addrlist,
                                 int* os_error) {
  // TODO(agayev): Implement raw DNS resolution.
  LOG(INFO) << "trying to resolve " << host;
  return net::ERR_NAME_NOT_RESOLVED;
}

RawHostResolverProc::~RawHostResolverProc() {}

}  // namespace chrome_common_net

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_RAW_HOST_RESOLVER_PROC_H_
#define CHROME_COMMON_NET_RAW_HOST_RESOLVER_PROC_H_
#pragma once

// RawHostResolverProc will eventually be a getaddrinfo() replacement.  It
// will construct and send DNS queries to the DNS server specified via
// --dns-server flag and will parse the responses and put it into a cache
// together with the TTL.  Necessary amendments will be made to cache and
// HostResolverProc interface to accomodate these.

#include <string>

#include "net/base/host_resolver_proc.h"
#include "net/base/net_util.h"

namespace chrome_common_net {

class RawHostResolverProc : public net::HostResolverProc {
 public:
  RawHostResolverProc(const net::IPAddressNumber& dns_server,
                      net::HostResolverProc* previous);

  virtual int Resolve(const std::string& host,
                      net::AddressFamily address_family,
                      net::HostResolverFlags host_resolver_flags,
                      net::AddressList* addrlist,
                      int* os_error);
 private:
  virtual ~RawHostResolverProc();

  net::IPAddressNumber dns_server_;
};

}  // namespace chrome_common_net

#endif  // CHROME_COMMON_NET_RAW_HOST_RESOLVER_PROC_H_

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/fixed_host_resolver.h"

#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/host_resolver_impl.h"

namespace net {

FixedHostResolver::FixedHostResolver(const std::string& host_and_port)
    : initialized_(false) {
  std::string host;
  int port = 0;
  if (!ParseHostAndPort(host_and_port, &host, &port)) {
    LOG(ERROR) << "Invalid FixedHostResolver information: " << host_and_port;
    return;
  }

  int rv = SystemHostResolverProc(host, net::ADDRESS_FAMILY_UNSPECIFIED,
                                  &address_);
  if (rv != OK) {
    LOG(ERROR) << "Could not resolve fixed host: " << host;
    return;
  }

  if (port <= 0) {
    LOG(ERROR) << "FixedHostResolver must contain a port number";
    return;
  }

  address_.SetPort(port);
  initialized_ = true;
}

int FixedHostResolver::Resolve(const RequestInfo& info,
                               AddressList* addresses,
                               CompletionCallback* callback,
                               RequestHandle* out_req,
                               LoadLog* load_log) {
  if (!initialized_)
    return ERR_NAME_NOT_RESOLVED;

  DCHECK(addresses);
  *addresses = address_;
  return OK;
}

}  // namespace net


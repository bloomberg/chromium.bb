// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/fixed_host_resolver.h"

#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/host_resolver_impl.h"

namespace net {

FixedHostResolver::FixedHostResolver(const std::string& host)
    : initialized_(false) {
  int port;
  std::string parsed_host;
  if (!ParseHostAndPort(host, &parsed_host, &port)) {
    LOG(DFATAL) << "Invalid FixedHostResolver information: " << host;
    return;
  }

  int rv = SystemHostResolverProc(host, net::ADDRESS_FAMILY_UNSPECIFIED,
                                  &address_);
  if (rv != OK) {
    LOG(ERROR) << "Could not resolve fixed host: " << host;
    return;
  }

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
  addresses->Copy(address_.head());
  addresses->SetPort(info.port());
  return OK;
}

}  // namespace net

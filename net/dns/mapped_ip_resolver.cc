// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mapped_ip_resolver.h"

#include "base/strings/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace net {

MappedIPResolver::MappedIPResolver(scoped_ptr<HostResolver> impl)
    : impl_(impl.Pass()),
      weak_factory_(this) {
}

MappedIPResolver::~MappedIPResolver() {}

int MappedIPResolver::Resolve(const RequestInfo& info,
                              RequestPriority priority,
                              AddressList* addresses,
                              const CompletionCallback& callback,
                              RequestHandle* out_req,
                              const BoundNetLog& net_log) {
  CompletionCallback new_callback = base::Bind(&MappedIPResolver::ApplyRules,
                                               weak_factory_.GetWeakPtr(),
                                               callback, addresses);
  int rv = impl_->Resolve(info, priority, addresses, new_callback, out_req,
                          net_log);
  if (rv == OK)
    rules_.RewriteAddresses(addresses);
  return rv;
}

int MappedIPResolver::ResolveFromCache(const RequestInfo& info,
                                       AddressList* addresses,
                                       const BoundNetLog& net_log) {
  int rv = impl_->ResolveFromCache(info, addresses, net_log);
  if (rv == OK)
    rules_.RewriteAddresses(addresses);
  return rv;
}

void MappedIPResolver::CancelRequest(RequestHandle req) {
  impl_->CancelRequest(req);
}

void MappedIPResolver::SetDnsClientEnabled(bool enabled) {
  impl_->SetDnsClientEnabled(enabled);
}

HostCache* MappedIPResolver::GetHostCache() {
  return impl_->GetHostCache();
}

base::Value* MappedIPResolver::GetDnsConfigAsValue() const {
  return impl_->GetDnsConfigAsValue();
}

void MappedIPResolver::ApplyRules(const CompletionCallback& original_callback,
                                  AddressList* addresses,
                                  int rv) const {
  if (rv == OK)
    rules_.RewriteAddresses(addresses);
  original_callback.Run(rv);
}

}  // namespace net

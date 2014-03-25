// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/dns/host_resolver_wrapper.h"

namespace extensions {

HostResolverWrapper::HostResolverWrapper() : resolver_(NULL) {}

// static
HostResolverWrapper* HostResolverWrapper::GetInstance() {
  return Singleton<extensions::HostResolverWrapper>::get();
}

net::HostResolver* HostResolverWrapper::GetHostResolver(
    net::HostResolver* real_resolver) {
  return resolver_ ? resolver_ : real_resolver;
}

void HostResolverWrapper::SetHostResolverForTesting(
    net::HostResolver* mock_resolver) {
  resolver_ = mock_resolver;
}

}  // namespace extensions

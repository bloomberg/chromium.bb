// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_factory.h"

namespace net {

ProxyResolverFactory::ProxyResolverFactory(bool resolvers_expect_pac_bytes)
    : resolvers_expect_pac_bytes_(resolvers_expect_pac_bytes) {
}

ProxyResolverFactory::~ProxyResolverFactory() {
}

}  // namespace net

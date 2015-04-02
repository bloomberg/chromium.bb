// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_FACTORY_H_
#define NET_PROXY_PROXY_RESOLVER_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace net {

class ProxyResolver;

// ProxyResolverFactory is an interface for creating ProxyResolver instances.
class NET_EXPORT ProxyResolverFactory {
 public:
  explicit ProxyResolverFactory(bool resolvers_expect_pac_bytes);

  virtual ~ProxyResolverFactory();

  // Creates a new ProxyResolver. The caller is responsible for freeing this
  // object.
  virtual scoped_ptr<ProxyResolver> CreateProxyResolver() = 0;

  bool resolvers_expect_pac_bytes() const {
    return resolvers_expect_pac_bytes_;
  }

 private:
  bool resolvers_expect_pac_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactory);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_FACTORY_H_

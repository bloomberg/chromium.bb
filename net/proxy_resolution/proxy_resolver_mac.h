// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLVER_MAC_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLVER_MAC_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "url/gurl.h"

namespace net {

// Implementation of ProxyResolverFactory that uses the Mac CFProxySupport to
// implement proxies.
// TODO(kapishnikov): make ProxyResolverMac async as per
// https://bugs.chromium.org/p/chromium/issues/detail?id=166387#c95
class NET_EXPORT ProxyResolverFactoryMac : public ProxyResolverFactory {
 public:
  ProxyResolverFactoryMac();

  int CreateProxyResolver(const scoped_refptr<PacFileData>& pac_script,
                          std::unique_ptr<ProxyResolver>* resolver,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryMac);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLVER_MAC_H_

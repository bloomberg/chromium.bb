// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_MOJO_PROXY_RESOLVER_IMPL_H_
#define NET_PROXY_MOJO_PROXY_RESOLVER_IMPL_H_

#include <map>
#include <queue>
#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"
#include "net/proxy/proxy_resolver.h"

namespace net {
class ProxyResolverV8Tracing;

class MojoProxyResolverImpl : public interfaces::ProxyResolver {
 public:
  explicit MojoProxyResolverImpl(scoped_ptr<ProxyResolverV8Tracing> resolver);

  ~MojoProxyResolverImpl() override;

 private:
  class Job;

  // interfaces::ProxyResolver overrides.
  void GetProxyForUrl(
      const mojo::String& url,
      interfaces::ProxyResolverRequestClientPtr client) override;

  void DeleteJob(Job* job);

  scoped_ptr<ProxyResolverV8Tracing> resolver_;
  std::set<Job*> resolve_jobs_;

  DISALLOW_COPY_AND_ASSIGN(MojoProxyResolverImpl);
};

}  // namespace net

#endif  // NET_PROXY_MOJO_PROXY_RESOLVER_IMPL_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_V8_TRACING_H_
#define NET_PROXY_PROXY_RESOLVER_V8_TRACING_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_factory.h"

namespace net {

class HostResolver;
class NetLog;
class ProxyResolverErrorObserver;

// ProxyResolverFactoryV8Tracing is a ProxyResolverFactory that returns
// non-blocking ProxyResolver instances. Each ProxyResolver instance executes
// ProxyResolverV8 on a single helper thread, and does some magic to avoid
// blocking in DNS. For more details see the design document:
// https://docs.google.com/a/google.com/document/d/16Ij5OcVnR3s0MH4Z5XkhI9VTPoMJdaBn9rKreAmGOdE/edit?pli=1
class NET_EXPORT ProxyResolverFactoryV8Tracing : public ProxyResolverFactory {
 public:
  // Note that |host_resolver| and |net_log| are expected to outlive |this| and
  // any ProxyResolver instances created using |this|. |error_observer_factory|
  // will be invoked once per CreateProxyResolver() call to create a
  // ProxyResolverErrorObserver to be used by the ProxyResolver instance
  // returned by that call.
  ProxyResolverFactoryV8Tracing(
      HostResolver* host_resolver,
      NetLog* net_log,
      const ProxyResolver::LoadStateChangedCallback& callback,
      const base::Callback<scoped_ptr<ProxyResolverErrorObserver>()>&
          error_observer_factory);
  ~ProxyResolverFactoryV8Tracing() override;

  // ProxyResolverFactory override.
  int CreateProxyResolver(
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      scoped_ptr<ProxyResolver>* resolver,
      const CompletionCallback& callback,
      scoped_ptr<Request>* request) override;

 private:
  class CreateJob;

  void RemoveJob(CreateJob* job);

  HostResolver* const host_resolver_;
  NetLog* const net_log_;
  const ProxyResolver::LoadStateChangedCallback load_state_changed_callback_;
  const base::Callback<scoped_ptr<ProxyResolverErrorObserver>()>
      error_observer_factory_;

  std::set<CreateJob*> jobs_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryV8Tracing);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_V8_TRACING_H_

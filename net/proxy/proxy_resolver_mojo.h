// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_MOJO_H_
#define NET_PROXY_PROXY_RESOLVER_MOJO_H_

#include <set>

#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/interfaces/host_resolver_service.mojom.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_factory.h"
#include "net/proxy/proxy_resolver_script_data.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

class GURL;

namespace net {

class BoundNetLog;
class HostResolver;
class ProxyInfo;
class MojoProxyResolverFactory;

// Implementation of ProxyResolver that connects to a Mojo service to evaluate
// PAC scripts. This implementation only knows about Mojo services, and
// therefore that service may live in or out of process.
//
// This implementation reports disconnections from the Mojo service (i.e. if the
// service is out-of-process and that process crashes) using the error code
// ERR_PAC_SCRIPT_TERMINATED.
class ProxyResolverMojo : public ProxyResolver, public mojo::ErrorHandler {
 public:
  // Constructs a ProxyResolverMojo that connects to a mojo proxy resolver
  // implementation using |resolver_ptr|. The implementation uses
  // |host_resolver| as the DNS resolver, using |host_resolver_binding| to
  // communicate with it. When deleted, the closure contained within
  // |on_delete_callback_runner| will be run.
  // TODO(amistry): Add ProxyResolverErrorObserver and NetLog.
  ProxyResolverMojo(
      interfaces::ProxyResolverPtr resolver_ptr,
      scoped_ptr<interfaces::HostResolver> host_resolver,
      scoped_ptr<mojo::Binding<interfaces::HostResolver>> host_resolver_binding,
      scoped_ptr<base::ScopedClosureRunner> on_delete_callback_runner);
  ~ProxyResolverMojo() override;

  // ProxyResolver implementation:
  int GetProxyForURL(const GURL& url,
                     ProxyInfo* results,
                     const net::CompletionCallback& callback,
                     RequestHandle* request,
                     const BoundNetLog& net_log) override;
  void CancelRequest(RequestHandle request) override;
  LoadState GetLoadState(RequestHandle request) const override;
  void CancelSetPacScript() override;
  int SetPacScript(const scoped_refptr<ProxyResolverScriptData>& pac_script,
                   const net::CompletionCallback& callback) override;

 private:
  class Job;

  // Overridden from mojo::ErrorHandler:
  void OnConnectionError() override;

  void RemoveJob(Job* job);

  // Connection to the Mojo proxy resolver.
  interfaces::ProxyResolverPtr mojo_proxy_resolver_ptr_;

  // Mojo host resolver service and binding.
  scoped_ptr<interfaces::HostResolver> mojo_host_resolver_;
  scoped_ptr<mojo::Binding<interfaces::HostResolver>>
      mojo_host_resolver_binding_;

  std::set<Job*> pending_jobs_;

  base::ThreadChecker thread_checker_;

  scoped_ptr<base::ScopedClosureRunner> on_delete_callback_runner_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverMojo);
};

// Implementation of ProxyResolverFactory that connects to a Mojo service to
// create implementations of a Mojo proxy resolver to back a ProxyResolverMojo.
class ProxyResolverFactoryMojo : public ProxyResolverFactory {
 public:
  ProxyResolverFactoryMojo(MojoProxyResolverFactory* mojo_proxy_factory,
                           HostResolver* host_resolver);
  ~ProxyResolverFactoryMojo() override;

  // ProxyResolverFactory override.
  int CreateProxyResolver(
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      scoped_ptr<ProxyResolver>* resolver,
      const CompletionCallback& callback,
      scoped_ptr<Request>* request) override;

 private:
  class Job;

  MojoProxyResolverFactory* const mojo_proxy_factory_;
  HostResolver* const host_resolver_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryMojo);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_MOJO_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_MOJO_H_
#define NET_PROXY_PROXY_RESOLVER_MOJO_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/interfaces/host_resolver_service.mojom.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"
#include "net/proxy/proxy_resolver.h"
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
// This implementation handles disconnections from the Mojo service (i.e. if the
// service is out-of-process and that process crashes) and transparently
// re-connects to a new service.
class ProxyResolverMojo : public ProxyResolver, public mojo::ErrorHandler {
 public:
  // Constructs a ProxyResolverMojo and connects to a new Mojo proxy resolver
  // service using |mojo_proxy_resolver_factory|. The new Mojo proxy resolver
  // uses |host_resolver| as the DNS resolver.  |mojo_proxy_resolver_factory|
  // and |host_resolver| are not owned and must outlive this.
  // TODO(amistry): Add ProxyResolverErrorObserver and NetLog.
  ProxyResolverMojo(MojoProxyResolverFactory* mojo_proxy_resolver_factory,
                    HostResolver* host_resolver);
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

  // Callback for ProxyResolverService::SetPacScript.
  void OnSetPacScriptDone(
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      const net::CompletionCallback& callback,
      int32_t result);

  void SetUpServices();

  void AbortPendingRequests();

  void RemoveJob(Job* job);

  // Connection to the Mojo proxy resolver.
  interfaces::ProxyResolverPtr mojo_proxy_resolver_ptr_;

  // Mojo host resolver service and binding.
  scoped_ptr<interfaces::HostResolver> mojo_host_resolver_;
  scoped_ptr<mojo::Binding<interfaces::HostResolver>>
      mojo_host_resolver_binding_;

  // Factory for connecting to new Mojo proxy resolvers.
  // Not owned.
  MojoProxyResolverFactory* mojo_proxy_resolver_factory_;

  // DNS resolver, saved for creating a new Mojo proxy resolver when the
  // existing one disconnects (i.e. when utility process crashes).
  HostResolver* host_resolver_;

  std::set<Job*> pending_jobs_;
  net::CancelableCompletionCallback set_pac_script_callback_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverMojo);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_MOJO_H_

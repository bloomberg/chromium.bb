// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_FACTORY_H_
#define CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_FACTORY_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "services/proxy_resolver/public/cpp/mojo_proxy_resolver_factory.h"

#if !defined(OS_ANDROID)
namespace content {
class UtilityProcessHost;
}
#endif

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}  // namespace base

// A factory used to create connections to Mojo proxy resolver services.  On
// Android, the proxy resolvers will run in the browser process, and on other
// platforms, they'll all be run in the same utility process. Utility process
// crashes are detected and the utility process is automatically restarted.
class ChromeMojoProxyResolverFactory
    : public proxy_resolver::MojoProxyResolverFactory {
 public:
  static ChromeMojoProxyResolverFactory* GetInstance();

  // Overridden from proxy_resolver::MojoProxyResolverFactory:
  std::unique_ptr<base::ScopedClosureRunner> CreateResolver(
      const std::string& pac_script,
      mojo::InterfaceRequest<proxy_resolver::mojom::ProxyResolver> req,
      proxy_resolver::mojom::ProxyResolverFactoryRequestClientPtr client)
      override;

 private:
  friend struct base::DefaultSingletonTraits<ChromeMojoProxyResolverFactory>;
  ChromeMojoProxyResolverFactory();
  ~ChromeMojoProxyResolverFactory() override;

  // Creates the proxy resolver factory. On desktop, creates a new utility
  // process before creating it out of process. On Android, creates it on the
  // current thread.
  void CreateFactory();

  // Destroys |resolver_factory_|.
  void DestroyFactory();

  // Invoked each time a proxy resolver is destroyed.
  void OnResolverDestroyed();

  // Invoked once an idle timeout has elapsed after all proxy resolvers are
  // destroyed.
  void OnIdleTimeout();

  proxy_resolver::mojom::ProxyResolverFactoryPtr resolver_factory_;

#if !defined(OS_ANDROID)
  base::WeakPtr<content::UtilityProcessHost> weak_utility_process_host_;
#endif

  size_t num_proxy_resolvers_ = 0;

  base::OneShotTimer idle_timer_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMojoProxyResolverFactory);
};

#endif  // CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_FACTORY_H_

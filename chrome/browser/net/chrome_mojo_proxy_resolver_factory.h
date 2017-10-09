// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_FACTORY_H_
#define CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_FACTORY_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/network/mojo_proxy_resolver_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}  // namespace base

// A factory used to create connections to Mojo proxy resolver services.  On
// Android, the proxy resolvers will run in the browser process, and on other
// platforms, they'll all be run in the same utility process.
class ChromeMojoProxyResolverFactory
    : public content::MojoProxyResolverFactory {
 public:
  static ChromeMojoProxyResolverFactory* GetInstance();

  // Overridden from proxy_content::MojoProxyResolverFactory:
  std::unique_ptr<base::ScopedClosureRunner> CreateResolver(
      const std::string& pac_script,
      mojo::InterfaceRequest<proxy_resolver::mojom::ProxyResolver> req,
      proxy_resolver::mojom::ProxyResolverFactoryRequestClientPtr client)
      override;

  // Used by tests to override the default timeout used when no resolver is
  // connected before disconnecting the factory (and causing the service to
  // stop).
  void SetFactoryIdleTimeoutForTests(const base::TimeDelta& timeout);

 private:
  friend struct base::DefaultSingletonTraits<ChromeMojoProxyResolverFactory>;
  ChromeMojoProxyResolverFactory();
  ~ChromeMojoProxyResolverFactory() override;

  // Initializes the ServiceManager's connector if it hasn't been already.
  void InitServiceManagerConnector();

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

  std::unique_ptr<service_manager::Connector> service_manager_connector_;

  size_t num_proxy_resolvers_ = 0;

  base::OneShotTimer idle_timer_;

  base::ThreadChecker thread_checker_;

  base::TimeDelta factory_idle_timeout_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMojoProxyResolverFactory);
};

#endif  // CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_FACTORY_H_

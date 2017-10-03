// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_factory.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"

#if !defined(OS_ANDROID)
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/l10n/l10n_util.h"
#else  // defined(OS_ANDROID)
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/proxy_resolver/public/cpp/mojo_proxy_resolver_factory_impl.h"
#endif  // !defined(OS_ANDROID)

namespace {
const int kUtilityProcessIdleTimeoutSeconds = 5;
}

// static
ChromeMojoProxyResolverFactory* ChromeMojoProxyResolverFactory::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return base::Singleton<
      ChromeMojoProxyResolverFactory,
      base::LeakySingletonTraits<ChromeMojoProxyResolverFactory>>::get();
}

ChromeMojoProxyResolverFactory::ChromeMojoProxyResolverFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

ChromeMojoProxyResolverFactory::~ChromeMojoProxyResolverFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

std::unique_ptr<base::ScopedClosureRunner>
ChromeMojoProxyResolverFactory::CreateResolver(
    const std::string& pac_script,
    mojo::InterfaceRequest<proxy_resolver::mojom::ProxyResolver> req,
    proxy_resolver::mojom::ProxyResolverFactoryRequestClientPtr client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!resolver_factory_)
    CreateFactory();

  if (!resolver_factory_) {
    // If factory creation failed, close |req|'s message pipe, which should
    // cause a connection error.
    req = nullptr;
    return nullptr;
  }
  idle_timer_.Stop();
  num_proxy_resolvers_++;
  resolver_factory_->CreateResolver(pac_script, std::move(req),
                                    std::move(client));
  return base::MakeUnique<base::ScopedClosureRunner>(
      base::Bind(&ChromeMojoProxyResolverFactory::OnResolverDestroyed,
                 base::Unretained(this)));
}

void ChromeMojoProxyResolverFactory::CreateFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!resolver_factory_);

#if defined(OS_ANDROID)
  mojo::MakeStrongBinding(
      base::MakeUnique<proxy_resolver::MojoProxyResolverFactoryImpl>(),
      mojo::MakeRequest(&resolver_factory_));
#else   // !defined(OS_ANDROID)
  DCHECK(!weak_utility_process_host_);

  DVLOG(1) << "Attempting to create utility process for proxy resolver";
  content::UtilityProcessHost* utility_process_host =
      content::UtilityProcessHost::Create(
          scoped_refptr<content::UtilityProcessHostClient>(),
          base::ThreadTaskRunnerHandle::Get());
  utility_process_host->SetName(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_PROXY_RESOLVER_NAME));
  bool process_started = utility_process_host->Start();
  if (process_started) {
    BindInterface(utility_process_host, &resolver_factory_);
    weak_utility_process_host_ = utility_process_host->AsWeakPtr();
  } else {
    LOG(ERROR) << "Unable to connect to utility process";
    return;
  }
#endif  // defined(OS_ANDROID)

  resolver_factory_.set_connection_error_handler(base::Bind(
      &ChromeMojoProxyResolverFactory::DestroyFactory, base::Unretained(this)));
}

void ChromeMojoProxyResolverFactory::DestroyFactory() {
  resolver_factory_.reset();
#if !defined(OS_ANDROID)
  delete weak_utility_process_host_.get();
  weak_utility_process_host_.reset();
#endif
}

void ChromeMojoProxyResolverFactory::OnResolverDestroyed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(num_proxy_resolvers_, 0u);
  if (--num_proxy_resolvers_ == 0) {
    // When all proxy resolvers have been destroyed, the proxy resolver factory
    // is no longer needed. However, new proxy resolvers may be created
    // shortly after being destroyed (e.g. due to a network change).
    //
    // On desktop, where a utility process is used, if the utility process is
    // shut down immediately, this would cause unnecessary process churn, so
    // wait for an idle timeout before shutting down the proxy resolver utility
    // process.
    idle_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kUtilityProcessIdleTimeoutSeconds), this,
        &ChromeMojoProxyResolverFactory::OnIdleTimeout);
  }
}

void ChromeMojoProxyResolverFactory::OnIdleTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(num_proxy_resolvers_, 0u);
  DestroyFactory();
}

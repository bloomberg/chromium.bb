// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/utility_process_mojo_proxy_resolver_factory.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const int kUtilityProcessIdleTimeoutSeconds = 5;
}

// static
UtilityProcessMojoProxyResolverFactory*
UtilityProcessMojoProxyResolverFactory::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return base::Singleton<UtilityProcessMojoProxyResolverFactory,
                         base::LeakySingletonTraits<
                             UtilityProcessMojoProxyResolverFactory>>::get();
}

UtilityProcessMojoProxyResolverFactory::
    UtilityProcessMojoProxyResolverFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

UtilityProcessMojoProxyResolverFactory::
    ~UtilityProcessMojoProxyResolverFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void UtilityProcessMojoProxyResolverFactory::CreateProcessAndConnect() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!resolver_factory_);
  DCHECK(!weak_utility_process_host_);
  DVLOG(1) << "Attempting to create utility process for proxy resolver";
  content::UtilityProcessHost* utility_process_host =
      content::UtilityProcessHost::Create(
          scoped_refptr<content::UtilityProcessHostClient>(),
          base::ThreadTaskRunnerHandle::Get());
  utility_process_host->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_PROXY_RESOLVER_NAME));
  bool process_started = utility_process_host->Start();
  if (process_started) {
    utility_process_host->GetRemoteInterfaces()->GetInterface(
        &resolver_factory_);
    resolver_factory_.set_connection_error_handler(
        base::Bind(&UtilityProcessMojoProxyResolverFactory::OnConnectionError,
                   base::Unretained(this)));
    weak_utility_process_host_ = utility_process_host->AsWeakPtr();
  } else {
    LOG(ERROR) << "Unable to connect to utility process";
  }
}

std::unique_ptr<base::ScopedClosureRunner>
UtilityProcessMojoProxyResolverFactory::CreateResolver(
    const std::string& pac_script,
    mojo::InterfaceRequest<net::interfaces::ProxyResolver> req,
    net::interfaces::ProxyResolverFactoryRequestClientPtr client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!resolver_factory_)
    CreateProcessAndConnect();

  if (!resolver_factory_) {
    // If there's still no factory, then utility process creation failed so
    // close |req|'s message pipe, which should cause a connection error.
    req = nullptr;
    return nullptr;
  }
  idle_timer_.Stop();
  num_proxy_resolvers_++;
  resolver_factory_->CreateResolver(pac_script, std::move(req),
                                    std::move(client));
  return base::MakeUnique<base::ScopedClosureRunner>(
      base::Bind(&UtilityProcessMojoProxyResolverFactory::OnResolverDestroyed,
                 base::Unretained(this)));
}

void UtilityProcessMojoProxyResolverFactory::OnConnectionError() {
  DVLOG(1) << "Disconnection from utility process detected";
  resolver_factory_.reset();
  delete weak_utility_process_host_.get();
  weak_utility_process_host_.reset();
}

void UtilityProcessMojoProxyResolverFactory::OnResolverDestroyed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(num_proxy_resolvers_, 0u);
  if (--num_proxy_resolvers_ == 0) {
    // When all proxy resolvers have been destroyed, the proxy resolver utility
    // process is no longer needed. However, new proxy resolvers may be created
    // shortly after being destroyed (e.g. due to a network change). If the
    // utility process is shut down immediately, this would cause unnecessary
    // process churn, so wait for an idle timeout before shutting down the
    // proxy resolver utility process.
    idle_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kUtilityProcessIdleTimeoutSeconds), this,
        &UtilityProcessMojoProxyResolverFactory::OnIdleTimeout);
  }
}

void UtilityProcessMojoProxyResolverFactory::OnIdleTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(num_proxy_resolvers_, 0u);
  delete weak_utility_process_host_.get();
  weak_utility_process_host_.reset();
  resolver_factory_.reset();
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"

namespace {

constexpr base::TimeDelta kUtilityProcessIdleTimeout =
    base::TimeDelta::FromSeconds(5);

void BindConnectorOnUIThread(service_manager::mojom::ConnectorRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindConnectorRequest(std::move(request));
}

}  // namespace

// static
ChromeMojoProxyResolverFactory* ChromeMojoProxyResolverFactory::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return base::Singleton<
      ChromeMojoProxyResolverFactory,
      base::LeakySingletonTraits<ChromeMojoProxyResolverFactory>>::get();
}

ChromeMojoProxyResolverFactory::ChromeMojoProxyResolverFactory()
    : factory_idle_timeout_(kUtilityProcessIdleTimeout) {
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

void ChromeMojoProxyResolverFactory::SetFactoryIdleTimeoutForTests(
    const base::TimeDelta& timeout) {
  factory_idle_timeout_ = timeout;
}

void ChromeMojoProxyResolverFactory::InitServiceManagerConnector() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (service_manager_connector_)
    return;

  // The existing ServiceManagerConnection retrieved with
  // ServiceManagerConnection::GetForProcess() lives on the UI thread, so we
  // can't access it from here. We create our own connector so it can be used
  // right away and will bind it on the UI thread.
  service_manager::mojom::ConnectorRequest request;
  service_manager_connector_ = service_manager::Connector::Create(&request);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&BindConnectorOnUIThread, base::Passed(&request)));
}

void ChromeMojoProxyResolverFactory::CreateFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!resolver_factory_);

  InitServiceManagerConnector();

  service_manager_connector_->BindInterface(
      proxy_resolver::mojom::kProxyResolverServiceName,
      mojo::MakeRequest(&resolver_factory_));

  resolver_factory_.set_connection_error_handler(base::Bind(
      &ChromeMojoProxyResolverFactory::DestroyFactory, base::Unretained(this)));
}

void ChromeMojoProxyResolverFactory::DestroyFactory() {
  resolver_factory_.reset();
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
    idle_timer_.Start(FROM_HERE, factory_idle_timeout_, this,
                      &ChromeMojoProxyResolverFactory::OnIdleTimeout);
  }
}

void ChromeMojoProxyResolverFactory::OnIdleTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(num_proxy_resolvers_, 0u);
  DestroyFactory();
}

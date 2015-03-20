// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/utility_process_mojo_proxy_resolver_factory.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/service_registry.h"
#include "net/proxy/mojo_proxy_resolver_factory.h"
#include "ui/base/l10n/l10n_util.h"

// static
UtilityProcessMojoProxyResolverFactory*
UtilityProcessMojoProxyResolverFactory::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return Singleton<
      UtilityProcessMojoProxyResolverFactory,
      LeakySingletonTraits<UtilityProcessMojoProxyResolverFactory>>::get();
}

UtilityProcessMojoProxyResolverFactory::
    UtilityProcessMojoProxyResolverFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

UtilityProcessMojoProxyResolverFactory::
    ~UtilityProcessMojoProxyResolverFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void UtilityProcessMojoProxyResolverFactory::CreateProcessAndConnect() {
  DVLOG(1) << "Attempting to create utility process for proxy resolver";
  content::UtilityProcessHost* utility_process_host =
      content::UtilityProcessHost::Create(
          scoped_refptr<content::UtilityProcessHostClient>(),
          base::MessageLoopProxy::current().get());
  utility_process_host->SetName(l10n_util::GetStringUTF8(
      IDS_UTILITY_PROCESS_PROXY_RESOLVER_NAME));
  bool process_started = utility_process_host->StartMojoMode();
  if (process_started) {
    content::ServiceRegistry* service_registry =
        utility_process_host->GetServiceRegistry();
    service_registry->ConnectToRemoteService(&resolver_factory_);
    resolver_factory_.set_error_handler(this);
  } else {
    LOG(ERROR) << "Unable to connect to utility process";
  }
}

void UtilityProcessMojoProxyResolverFactory::Create(
    mojo::InterfaceRequest<net::interfaces::ProxyResolver> req,
    net::interfaces::HostResolverPtr host_resolver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!resolver_factory_)
    CreateProcessAndConnect();

  if (!resolver_factory_) {
    // If there's still no factory, then utility process creation failed so
    // close |req|'s message pipe, which should cause a connection error.
    req = nullptr;
    return;
  }
  resolver_factory_->CreateResolver(req.Pass(), host_resolver.Pass());
}

void UtilityProcessMojoProxyResolverFactory::OnConnectionError() {
  DVLOG(1) << "Disconnection from utility process detected";
  resolver_factory_.reset();
}

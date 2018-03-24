// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/system_network_context_manager.h"

#include <string>

#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/default_network_context_params.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "net/net_buildflags.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"

namespace {

// Called on IOThread to disable QUIC for HttpNetworkSessions not using the
// network service. Note that re-enabling QUIC dynamically is not supported for
// simpliciy and requires a browser restart.
void DisableQuicOnIOThread(
    IOThread* io_thread,
    safe_browsing::SafeBrowsingService* safe_browsing_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    content::GetNetworkServiceImpl()->DisableQuic();
  io_thread->DisableQuic();

  // Safebrowsing isn't yet using the IOThread's NetworkService, so must be
  // handled separately.
  safe_browsing_service->DisableQuicOnIOThread();
}

}  // namespace

base::LazyInstance<SystemNetworkContextManager>::Leaky
    g_system_network_context_manager = LAZY_INSTANCE_INITIALIZER;

network::mojom::NetworkContext* SystemNetworkContextManager::GetContext() {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // SetUp should already have been called.
    DCHECK(io_thread_network_context_);
    return io_thread_network_context_.get();
  }

  if (!network_service_network_context_ ||
      network_service_network_context_.encountered_error()) {
    network::mojom::NetworkService* network_service =
        content::GetNetworkService();
    if (!is_quic_allowed_)
      network_service->DisableQuic();
    network_service->CreateNetworkContext(
        MakeRequest(&network_service_network_context_),
        CreateNetworkContextParams());
  }
  return network_service_network_context_.get();
}

network::mojom::URLLoaderFactory*
SystemNetworkContextManager::GetURLLoaderFactory() {
  if (!url_loader_factory_ || url_loader_factory_.encountered_error()) {
    GetContext()->CreateURLLoaderFactory(
        mojo::MakeRequest(&url_loader_factory_), 0);
  }
  return url_loader_factory_.get();
}

void SystemNetworkContextManager::SetUp(
    network::mojom::NetworkContextRequest* network_context_request,
    network::mojom::NetworkContextParamsPtr* network_context_params,
    bool* is_quic_allowed) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    *network_context_request = mojo::MakeRequest(&io_thread_network_context_);
    *network_context_params = CreateNetworkContextParams();
  } else {
    // Just use defaults if the network service is enabled, since
    // CreateNetworkContextParams() can only be called once.
    *network_context_params = CreateDefaultNetworkContextParams();
  }
  *is_quic_allowed = is_quic_allowed_;
}

SystemNetworkContextManager::SystemNetworkContextManager() {
  const base::Value* value =
      g_browser_process->policy_service()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()))
          .GetValue(policy::key::kQuicAllowed);
  if (value)
    value->GetAsBoolean(&is_quic_allowed_);
}

SystemNetworkContextManager::~SystemNetworkContextManager() {}

void SystemNetworkContextManager::DisableQuic() {
  is_quic_allowed_ = false;

  // Disabling QUIC for a profile disables QUIC globally. As a side effect, new
  // Profiles will also have QUIC disabled (because both IOThread's
  // NetworkService and the network service, if enabled will disable QUIC).

  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    content::GetNetworkService()->DisableQuic();

  IOThread* io_thread = g_browser_process->io_thread();
  // Nothing more to do if IOThread has already been shut down.
  if (!io_thread)
    return;

  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DisableQuicOnIOThread, io_thread,
                     base::Unretained(safe_browsing_service)));
}

void SystemNetworkContextManager::FlushProxyConfigMonitorForTesting() {
  proxy_config_monitor_.FlushForTesting();
}

void SystemNetworkContextManager::FlushNetworkInterfaceForTesting() {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    DCHECK(io_thread_network_context_);
    io_thread_network_context_.FlushForTesting();
  } else {
    DCHECK(network_service_network_context_);
    network_service_network_context_.FlushForTesting();
  }
  if (url_loader_factory_)
    url_loader_factory_.FlushForTesting();
}

network::mojom::NetworkContextParamsPtr
SystemNetworkContextManager::CreateNetworkContextParams() {
  // TODO(mmenke): Set up parameters here (in memory cookie store, etc).
  network::mojom::NetworkContextParamsPtr network_context_params =
      CreateDefaultNetworkContextParams();

  network_context_params->context_name = std::string("system");

  network_context_params->http_cache_enabled = false;

  // These are needed for PAC scripts that use file or data URLs (Or FTP URLs?).
  network_context_params->enable_data_url_support = true;
  network_context_params->enable_file_url_support = true;
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  network_context_params->enable_ftp_url_support = true;
#endif

  proxy_config_monitor_.AddToNetworkContextParams(network_context_params.get());

  return network_context_params;
}

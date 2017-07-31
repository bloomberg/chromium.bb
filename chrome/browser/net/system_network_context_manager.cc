// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/system_network_context_manager.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"

namespace {

content::mojom::NetworkContextParamsPtr CreateNetworkContextParams() {
  // TODO(mmenke): Set up parameters here (No cache, in memory cookie store,
  // etc).
  content::mojom::NetworkContextParamsPtr network_context_params =
      content::mojom::NetworkContextParams::New();

  // These are needed for PAC scripts that use file or data URLs (Or FTP URLs?).
  network_context_params->enable_data_url_support = true;
  network_context_params->enable_file_url_support = true;
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  network_context_params->enable_ftp_url_support = true;
#endif

  return network_context_params;
}

}  // namespace

base::LazyInstance<SystemNetworkContextManager>::Leaky
    g_system_network_context_manager = LAZY_INSTANCE_INITIALIZER;

content::mojom::NetworkContext* SystemNetworkContextManager::Context() {
  return GetInstance()->GetContext();
}

void SystemNetworkContextManager::SetUp(
    content::mojom::NetworkContextRequest* network_context_request,
    content::mojom::NetworkContextParamsPtr* network_context_params) {
  DCHECK(!GetInstance()->io_thread_network_context_);
  *network_context_request =
      mojo::MakeRequest(&GetInstance()->io_thread_network_context_);
  *network_context_params = CreateNetworkContextParams();
}

SystemNetworkContextManager* SystemNetworkContextManager::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_system_network_context_manager.Pointer();
}

SystemNetworkContextManager::SystemNetworkContextManager() {}

SystemNetworkContextManager::~SystemNetworkContextManager() {}

content::mojom::NetworkContext* SystemNetworkContextManager::GetContext() {
  if (!base::FeatureList::IsEnabled(features::kNetworkService)) {
    // SetUp should already have been called.
    DCHECK(io_thread_network_context_);
    return io_thread_network_context_.get();
  }

  if (!network_service_network_context_) {
    content::GetNetworkService()->CreateNetworkContext(
        MakeRequest(&network_service_network_context_),
        CreateNetworkContextParams());
  }
  return network_service_network_context_.get();
}

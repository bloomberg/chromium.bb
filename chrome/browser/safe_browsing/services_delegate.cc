// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/services_delegate.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/telemetry/telemetry_service.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/safe_browsing_network_context.h"
#include "components/safe_browsing/core/db/v4_local_database_manager.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

namespace safe_browsing {

ServicesDelegate::ServicesDelegate(SafeBrowsingService* safe_browsing_service,
                                   ServicesCreator* services_creator)
    : safe_browsing_service_(safe_browsing_service),
      services_creator_(services_creator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ServicesDelegate::~ServicesDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ServicesDelegate::CreatePasswordProtectionService(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  auto it = password_protection_service_map_.find(profile);
  DCHECK(it == password_protection_service_map_.end());
  auto service = std::make_unique<ChromePasswordProtectionService>(
      safe_browsing_service_, profile);
  password_protection_service_map_[profile] = std::move(service);
}

void ServicesDelegate::RemovePasswordProtectionService(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  auto it = password_protection_service_map_.find(profile);
  if (it != password_protection_service_map_.end())
    password_protection_service_map_.erase(it);
}

PasswordProtectionService* ServicesDelegate::GetPasswordProtectionService(
    Profile* profile) const {
  DCHECK(profile);
  auto it = password_protection_service_map_.find(profile);
  return it != password_protection_service_map_.end() ? it->second.get()
                                                      : nullptr;
}

void ServicesDelegate::ShutdownServices() {
  // Delete the ChromePasswordProtectionService instances.
  password_protection_service_map_.clear();

  // Delete the NetworkContexts and associated ProxyConfigMonitors
  network_context_map_.clear();
  proxy_config_monitor_map_.clear();
}

void ServicesDelegate::CreateSafeBrowsingNetworkContext(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  if (!base::FeatureList::IsEnabled(kSafeBrowsingSeparateNetworkContexts))
    return;

  auto it = network_context_map_.find(profile);
  DCHECK(it == network_context_map_.end());
  network_context_map_[profile] = std::make_unique<SafeBrowsingNetworkContext>(
      profile->GetPath(),
      base::BindRepeating(&ServicesDelegate::CreateNetworkContextParams,
                          base::Unretained(this), profile));
  proxy_config_monitor_map_[profile] =
      std::make_unique<ProxyConfigMonitor>(profile);
}

void ServicesDelegate::RemoveSafeBrowsingNetworkContext(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  if (!base::FeatureList::IsEnabled(kSafeBrowsingSeparateNetworkContexts))
    return;

  auto it = network_context_map_.find(profile);
  if (it != network_context_map_.end()) {
    it->second->ServiceShuttingDown();
    network_context_map_.erase(it);
  }

  auto proxy_it = proxy_config_monitor_map_.find(profile);
  if (proxy_it != proxy_config_monitor_map_.end())
    proxy_config_monitor_map_.erase(proxy_it);
}

SafeBrowsingNetworkContext* ServicesDelegate::GetSafeBrowsingNetworkContext(
    Profile* profile) const {
  DCHECK(profile);
  DCHECK(base::FeatureList::IsEnabled(kSafeBrowsingSeparateNetworkContexts));
  auto it = network_context_map_.find(profile);
  DCHECK(it != network_context_map_.end());
  return it->second.get();
}

network::mojom::NetworkContextParamsPtr
ServicesDelegate::CreateNetworkContextParams(Profile* profile) {
  auto params = SystemNetworkContextManager::GetInstance()
                    ->CreateDefaultNetworkContextParams();
  auto it = proxy_config_monitor_map_.find(profile);
  DCHECK(it != proxy_config_monitor_map_.end());
  it->second->AddToNetworkContextParams(params.get());
  return params;
}

}  // namespace safe_browsing

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/networking_config_delegate_chromeos.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "extensions/browser/api/networking_config/networking_config_service.h"
#include "extensions/browser/api/networking_config/networking_config_service_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace chromeos {

NetworkingConfigDelegateChromeos::NetworkingConfigDelegateChromeos() {
}

NetworkingConfigDelegateChromeos::~NetworkingConfigDelegateChromeos() {
}

scoped_ptr<const ash::NetworkingConfigDelegate::ExtensionInfo>
NetworkingConfigDelegateChromeos::LookUpExtensionForNetwork(
    const std::string& service_path) {
  chromeos::NetworkStateHandler* handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  const chromeos::NetworkState* network_state =
    handler->GetNetworkState(service_path);
  if (!network_state)
    return nullptr;
  std::string hex_ssid = network_state->GetHexSsid();
  Profile* profile = ProfileManager::GetActiveUserProfile();
  extensions::NetworkingConfigService* networking_config_service =
      extensions::NetworkingConfigServiceFactory::GetForBrowserContext(profile);
  const std::string extension_id =
      networking_config_service->LookupExtensionIdForHexSsid(hex_ssid);
  if (extension_id.empty())
    return nullptr;
  std::string extension_name = LookUpExtensionName(profile, extension_id);
  if (extension_name.empty())
    return scoped_ptr<const ash::NetworkingConfigDelegate::ExtensionInfo>();
  scoped_ptr<const ash::NetworkingConfigDelegate::ExtensionInfo> extension_info(
      new const ash::NetworkingConfigDelegate::ExtensionInfo(extension_id,
                                                             extension_name));
  return extension_info;
}

std::string NetworkingConfigDelegateChromeos::LookUpExtensionName(
    content::BrowserContext* context,
    std::string extension_id) const {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(context);
  DCHECK(extension_registry);
  const extensions::Extension* extension = extension_registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);
  if (extension == nullptr)
    return std::string();
  return extension->name();
}

}  // namespace chromeos

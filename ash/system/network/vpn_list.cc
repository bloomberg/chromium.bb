// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_list.h"

#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/logging.h"

namespace ash {

VPNProvider::VPNProvider() = default;

VPNProvider VPNProvider::CreateBuiltInVPNProvider() {
  VPNProvider vpn_provider;
  vpn_provider.provider_type = BUILT_IN_VPN;
  return vpn_provider;
}

VPNProvider VPNProvider::CreateExtensionVPNProvider(
    const std::string& extension_id,
    const std::string& third_party_provider_name) {
  DCHECK(!extension_id.empty());
  DCHECK(!third_party_provider_name.empty());

  VPNProvider vpn_provider;
  vpn_provider.provider_type = THIRD_PARTY_VPN;
  vpn_provider.app_id = extension_id;
  vpn_provider.provider_name = third_party_provider_name;
  return vpn_provider;
}

VPNProvider VPNProvider::CreateArcVPNProvider(
    const std::string& package_name,
    const std::string& app_name,
    const std::string& app_id,
    const base::Time last_launch_time) {
  DCHECK(!app_id.empty());
  DCHECK(!app_name.empty());
  DCHECK(!package_name.empty());
  DCHECK(!last_launch_time.is_null());

  VPNProvider vpn_provider;
  vpn_provider.provider_type = ARC_VPN;
  vpn_provider.app_id = app_id;
  vpn_provider.provider_name = app_name;
  vpn_provider.package_name = package_name;
  vpn_provider.last_launch_time = last_launch_time;
  return vpn_provider;
}

VPNProvider::VPNProvider(const VPNProvider& other) {
  provider_type = other.provider_type;
  app_id = other.app_id;
  provider_name = other.provider_name;
  package_name = other.package_name;
  last_launch_time = other.last_launch_time;
}

bool VPNProvider::operator==(const VPNProvider& other) const {
  return provider_type == other.provider_type && app_id == other.app_id &&
         provider_name == other.provider_name &&
         package_name == other.package_name;
}

VpnList::Observer::~Observer() = default;

VpnList::VpnList() {
  AddBuiltInProvider();

  ash::GetNetworkConfigService(
      cros_network_config_.BindNewPipeAndPassReceiver());
  chromeos::network_config::mojom::CrosNetworkConfigObserverPtr observer_ptr;
  cros_network_config_observer_.Bind(mojo::MakeRequest(&observer_ptr));
  cros_network_config_->AddObserver(std::move(observer_ptr));
  OnVpnProvidersChanged();
}

VpnList::~VpnList() = default;

bool VpnList::HaveExtensionOrArcVPNProviders() const {
  for (const VPNProvider& extension_provider : extension_vpn_providers_) {
    if (extension_provider.provider_type == VPNProvider::THIRD_PARTY_VPN)
      return true;
  }
  return arc_vpn_providers_.size() > 0;
}

void VpnList::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VpnList::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void VpnList::OnActiveNetworksChanged(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {}

void VpnList::OnNetworkStateChanged(
    chromeos::network_config::mojom::NetworkStatePropertiesPtr network) {}

void VpnList::OnNetworkStateListChanged() {}

void VpnList::OnDeviceStateListChanged() {}

void VpnList::OnVpnProvidersChanged() {
  cros_network_config_->GetVpnProviders(
      base::BindOnce(&VpnList::OnGetVpnProviders, base::Unretained(this)));
}

void VpnList::SetVpnProvidersForTest(
    std::vector<chromeos::network_config::mojom::VpnProviderPtr> providers) {
  OnGetVpnProviders(std::move(providers));
}

void VpnList::OnGetVpnProviders(
    std::vector<chromeos::network_config::mojom::VpnProviderPtr> providers) {
  extension_vpn_providers_.clear();
  arc_vpn_providers_.clear();
  // Add the OpenVPN/L2TP provider.
  AddBuiltInProvider();
  // Add Third Party (Extension and Arc) providers.
  for (const auto& provider : providers) {
    switch (provider->type) {
      case chromeos::network_config::mojom::VpnType::kL2TPIPsec:
      case chromeos::network_config::mojom::VpnType::kOpenVPN:
        // Only third party VpnProvider instances should exist.
        NOTREACHED();
        break;
      case chromeos::network_config::mojom::VpnType::kExtension:
        extension_vpn_providers_.push_back(
            VPNProvider::CreateExtensionVPNProvider(provider->provider_id,
                                                    provider->provider_name));
        break;
      case chromeos::network_config::mojom::VpnType::kArc:
        arc_vpn_providers_.push_back(VPNProvider::CreateArcVPNProvider(
            provider->provider_id, provider->provider_name, provider->app_id,
            provider->last_launch_time));
        break;
    }
  }
  NotifyObservers();
}

void VpnList::NotifyObservers() {
  for (auto& observer : observer_list_)
    observer.OnVPNProvidersChanged();
}

void VpnList::AddBuiltInProvider() {
  // The VPNProvider() constructor generates the built-in provider and has no
  // extension ID.
  extension_vpn_providers_.push_back(VPNProvider::CreateBuiltInVPNProvider());
}

}  // namespace ash

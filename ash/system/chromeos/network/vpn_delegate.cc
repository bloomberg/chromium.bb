// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/vpn_delegate.h"

#include "chromeos/network/network_state.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

VPNProvider::Key::Key() : third_party(false) {
}

VPNProvider::Key::Key(const std::string& extension_id)
    : third_party(true), extension_id(extension_id) {
}

bool VPNProvider::Key::operator==(const Key& other) const {
  return other.third_party == third_party && other.extension_id == extension_id;
}

bool VPNProvider::Key::MatchesNetwork(
    const chromeos::NetworkState& network) const {
  if (network.type() != shill::kTypeVPN)
    return false;
  const bool network_uses_third_party_provider =
      network.vpn_provider_type() == shill::kProviderThirdPartyVpn;
  if (!third_party)
    return !network_uses_third_party_provider;
  return network_uses_third_party_provider &&
         network.third_party_vpn_provider_extension_id() == extension_id;
}

VPNProvider::VPNProvider(const Key& key, const std::string& name)
    : key(key), name(name) {
}

VPNDelegate::Observer::~Observer() {
}

VPNDelegate::VPNDelegate() {
}

VPNDelegate::~VPNDelegate() {
}

void VPNDelegate::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VPNDelegate::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void VPNDelegate::NotifyObservers() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnVPNProvidersChanged());
}

}  // namespace ash

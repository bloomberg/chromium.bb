// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/vpn_delegate.h"

#include "base/logging.h"

namespace ash {

VPNProvider::VPNProvider() : third_party(false) {}

VPNProvider::VPNProvider(const std::string& extension_id,
                         const std::string& third_party_provider_name)
    : third_party(true),
      extension_id(extension_id),
      third_party_provider_name(third_party_provider_name) {
  DCHECK(!extension_id.empty());
  DCHECK(!third_party_provider_name.empty());
}

bool VPNProvider::operator==(const VPNProvider& other) const {
  return third_party == other.third_party &&
         extension_id == other.extension_id &&
         third_party_provider_name == other.third_party_provider_name;
}

VPNDelegate::Observer::~Observer() {}

VPNDelegate::VPNDelegate() {
  AddBuiltInProvider();
}

VPNDelegate::~VPNDelegate() {}

bool VPNDelegate::HaveThirdPartyVPNProviders() const {
  for (const VPNProvider& provider : vpn_providers_) {
    if (provider.third_party)
      return true;
  }
  return false;
}

void VPNDelegate::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VPNDelegate::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void VPNDelegate::SetThirdPartyVpnProviders(
    const std::vector<VPNProvider>& third_party_providers) {
  vpn_providers_.clear();
  vpn_providers_.reserve(third_party_providers.size() + 1);
  AddBuiltInProvider();
  // Append the extension-backed providers.
  vpn_providers_.insert(vpn_providers_.end(), third_party_providers.begin(),
                        third_party_providers.end());
  NotifyObservers();
}

void VPNDelegate::NotifyObservers() {
  for (auto& observer : observer_list_)
    observer.OnVPNProvidersChanged();
}

void VPNDelegate::AddBuiltInProvider() {
  // The VPNProvider() constructor generates the built-in provider and has no
  // extension ID.
  vpn_providers_.push_back(VPNProvider());
}

}  // namespace ash

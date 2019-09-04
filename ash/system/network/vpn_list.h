// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_VPN_LIST_H_
#define ASH_SYSTEM_NETWORK_VPN_LIST_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Describes a VPN provider for the UI. TODO(979314): Remove this class and use
// network_config::mojom::VpnProvider instead.
struct ASH_EXPORT VPNProvider {
  enum ProviderType {
    BUILT_IN_VPN = 0,
    THIRD_PARTY_VPN,
    ARC_VPN,
  };

  VPNProvider();

  static VPNProvider CreateBuiltInVPNProvider();
  static VPNProvider CreateExtensionVPNProvider(
      const std::string& extension_id,
      const std::string& third_party_provider_name);
  static VPNProvider CreateArcVPNProvider(const std::string& package_name,
                                          const std::string& app_name,
                                          const std::string& app_id,
                                          const base::Time last_launch_time);

  // Explicit copy constructor.
  VPNProvider(const VPNProvider& other);

  bool operator==(const VPNProvider& other) const;

  // This property represents whether this is a built-in or third-party or Arc
  // VPN provider.
  ProviderType provider_type;

  // Properties used by third-party VPN providers and Arc VPN providers. Empty
  // for built-in VPN.

  // App id of the extension or Arc app that implements this provider.
  std::string app_id;

  // Human-readable name.
  std::string provider_name;

  // Properties used by Arc VPN providers. Empty for built-in VPN and
  // third-party VPN providers.

  // Package name of the Arc VPN provider. e.g. package.name.foo.bar
  std::string package_name;

  // Last launch time is used to sort Arc VPN providers.
  base::Time last_launch_time;
};

// This delegate provides UI code in ash, e.g. |VPNListView|, with access to the
// list of VPN providers enabled in the primary user's profile. The delegate
// furthermore allows the UI code to request that a VPN provider show its "add
// network" dialog and allows UI code to request to launch Arc VPN provider.
class ASH_EXPORT VpnList
    : public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  // An observer that is notified whenever the list of VPN providers enabled in
  // the primary user's profile changes.
  class Observer {
   public:
    virtual void OnVPNProvidersChanged() = 0;

   protected:
    virtual ~Observer();

   private:
    DISALLOW_ASSIGN(Observer);
  };

  VpnList();
  ~VpnList() override;

  const std::vector<VPNProvider>& extension_vpn_providers() {
    return extension_vpn_providers_;
  }
  const std::vector<VPNProvider>& arc_vpn_providers() {
    return arc_vpn_providers_;
  }

  // Returns |true| if at least one third-party VPN provider or at least one Arc
  // VPN provider is enabled in the primary user's profile, in addition to the
  // built-in OpenVPN/L2TP provider.
  bool HaveExtensionOrArcVPNProviders() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // CrosNetworkConfigObserver
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override;
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;
  void OnVpnProvidersChanged() override;

  void SetVpnProvidersForTest(
      std::vector<chromeos::network_config::mojom::VpnProviderPtr> providers);

 private:
  void OnGetVpnProviders(
      std::vector<chromeos::network_config::mojom::VpnProviderPtr> providers);

  // Notify observers that the list of VPN providers enabled in the primary
  // user's profile has changed.
  void NotifyObservers();

  // Adds the built-in OpenVPN/L2TP provider to |extension_vpn_providers_|.
  void AddBuiltInProvider();

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_{this};

  // Cache of VPN providers, including the built-in OpenVPN/L2TP provider and
  // other providers added by extensions in the primary user's profile.
  std::vector<VPNProvider> extension_vpn_providers_;

  // Cache of Arc VPN providers. Will be sorted based on last launch time when
  // creating vpn list view.
  std::vector<VPNProvider> arc_vpn_providers_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(VpnList);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_VPN_LIST_H_

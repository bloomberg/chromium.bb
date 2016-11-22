// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_VPN_LIST_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_VPN_LIST_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/interfaces/vpn_list.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ash {

// Describes a VPN provider.
struct ASH_EXPORT VPNProvider {
  // Constructs the built-in VPN provider.
  VPNProvider();

  // Constructs a third-party VPN provider.
  VPNProvider(const std::string& extension_id,
              const std::string& third_party_provider_name);

  bool operator==(const VPNProvider& other) const;

  // Whether this key represents a built-in or third-party VPN provider.
  bool third_party;

  // ID of the extension that implements this provider. Used for third-party
  // VPN providers only.
  std::string extension_id;

  // Human-readable name if |third_party| is true, otherwise empty.
  std::string third_party_provider_name;
};

// This delegate provides UI code in ash, e.g. |VPNListView|, with access to the
// list of VPN providers enabled in the primary user's profile. The delegate
// furthermore allows the UI code to request that a VPN provider show its "add
// network" dialog.
class ASH_EXPORT VpnList : public mojom::VpnList {
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

  const std::vector<VPNProvider>& vpn_providers() { return vpn_providers_; }

  // Returns |true| if at least one third-party VPN provider is enabled in the
  // primary user's profile, in addition to the built-in OpenVPN/L2TP provider.
  bool HaveThirdPartyVPNProviders() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Binds the mojom::VpnList interface to this object.
  void BindRequest(mojom::VpnListRequest request);

  // mojom::VpnList:
  void SetThirdPartyVpnProviders(
      std::vector<mojom::ThirdPartyVpnProviderPtr> providers) override;

 private:
  // Notify observers that the list of VPN providers enabled in the primary
  // user's profile has changed.
  void NotifyObservers();

  // Adds the built-in OpenVPN/L2TP provider to |vpn_providers_|.
  void AddBuiltInProvider();

  // Bindings for the mojom::VpnList interface.
  mojo::BindingSet<mojom::VpnList> bindings_;

  // Cache of VPN providers, including the built-in OpenVPN/L2TP provider and
  // other providers added by extensions in the primary user's profile.
  std::vector<VPNProvider> vpn_providers_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(VpnList);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_VPN_LIST_H_

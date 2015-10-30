// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_VPN_DELEGATE_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_VPN_DELEGATE_H

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace chromeos {
class NetworkState;
}

namespace ash {

// Describes a VPN provider.
struct ASH_EXPORT VPNProvider {
  struct Key {
    // Constructs a key for the built-in VPN provider.
    Key();

    // Constructs a key for a third-party VPN provider.
    explicit Key(const std::string& extension_id);

    bool operator==(const Key& other) const;

    // Indicates whether |network| belongs to this VPN provider.
    bool MatchesNetwork(const chromeos::NetworkState& network) const;

    // Whether this key represents a built-in or third-party VPN provider.
    bool third_party;

    // ID of the extension that implements this provider. Used for third-party
    // VPN providers only.
    std::string extension_id;
  };

  VPNProvider(const Key& key, const std::string& name);

  // Key that uniquely identifies this VPN provider.
  Key key;

  // Human-readable name.
  std::string name;
};

// This delegate provides UI code in ash, e.g. |VPNList|, with access to the
// list of VPN providers enabled in the primary user's profile. The delegate
// furthermore allows the UI code to request that a VPN provider show its "add
// network" dialog.
class ASH_EXPORT VPNDelegate {
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

  VPNDelegate();
  virtual ~VPNDelegate();

  // Returns |true| if at least one third-party VPN provider is enabled in the
  // primary user's profile, in addition to the built-in OpenVPN/L2TP provider.
  virtual bool HaveThirdPartyVPNProviders() const = 0;

  // Returns the list of VPN providers enabled in the primary user's profile.
  virtual const std::vector<VPNProvider>& GetVPNProviders() const = 0;

  // Requests that the VPN provider identified by |key| show its "add network"
  // dialog.
  virtual void ShowAddPage(const VPNProvider::Key& key) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Notify observers that the list of VPN providers enabled in the primary
  // user's profile has changed.
  void NotifyObservers();

 private:
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(VPNDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_VPN_DELEGATE_H

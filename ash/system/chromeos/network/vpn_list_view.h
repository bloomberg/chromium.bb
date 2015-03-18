// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_VPN_LIST_VIEW_H_
#define ASH_SYSTEM_CHROMEOS_NETWORK_VPN_LIST_VIEW_H_

#include <map>
#include <string>

#include "ash/system/chromeos/network/vpn_delegate.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/macros.h"
#include "chromeos/network/network_state_handler.h"
#include "ui/chromeos/network/network_list_view_base.h"

namespace chromeos {
class NetworkState;
}

namespace ui {
class NetworkListDelegate;
}

namespace views {
class View;
}

namespace ash {

// A list of VPN providers and networks that shows VPN providers and networks in
// a hierarchical layout, allowing the user to see at a glance which provider a
// network belongs to. The only exception is the currently connected or
// connecting network, which is detached from its provider and moved to the top.
// If there is a connected network, a disconnect button is shown next to its
// name.
//
// Disconnected networks are arranged in shill's priority order within each
// provider and the providers are arranged in the order of their highest
// priority network. Clicking on a disconnected network triggers a connection
// attempt. Clicking on the currently connected or connecting network shows its
// configuration dialog. Clicking on a provider shows the provider's "add
// network" dialog.
class VPNListView : public ui::NetworkListViewBase,
                    public VPNDelegate::Observer,
                    public ViewClickListener {
 public:
  explicit VPNListView(ui::NetworkListDelegate* delegate);
  ~VPNListView() override;

  // ui::NetworkListViewBase:
  void Update() override;
  bool IsNetworkEntry(views::View* view,
                      std::string* service_path) const override;

  // VPNDelegate::Observer:
  void OnVPNProvidersChanged() override;

  // ViewClickListener:
  void OnViewClicked(views::View* sender) override;

 private:
  // Adds a network to the list.
  void AddNetwork(const chromeos::NetworkState* network);

  // Adds the VPN provider identified by |key| to the list, along with any
  // networks that belong to this provider.
  void AddProviderAndNetworks(
      const VPNProvider::Key& key,
      const std::string& name,
      const chromeos::NetworkStateHandler::NetworkStateList& networks);

  // Adds all available VPN providers and networks to the list.
  void AddProvidersAndNetworks(
      const chromeos::NetworkStateHandler::NetworkStateList& networks);

  ui::NetworkListDelegate* const delegate_;

  // A mapping from each VPN provider's list entry to the provider's key.
  std::map<const views::View* const, VPNProvider::Key> provider_view_key_map_;

  // A mapping from each network's list entry to the network's service path.
  std::map<const views::View* const, std::string>
      network_view_service_path_map_;

  // Whether the list is currently empty (i.e., the next entry added will become
  // the topmost entry).
  bool list_empty_ = true;

  DISALLOW_COPY_AND_ASSIGN(VPNListView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_VPN_LIST_VIEW_H_

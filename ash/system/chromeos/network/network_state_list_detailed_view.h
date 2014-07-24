// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H

#include <string>

#include "ash/system/chromeos/network/network_detailed_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "ash/system/user/login_status.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "ui/chromeos/network/network_list.h"
#include "ui/chromeos/network/network_list_delegate.h"
#include "ui/views/controls/button/button.h"

namespace chromeos {
class NetworkTypePattern;
}

namespace views {
class BubbleDelegateView;
}

namespace ash {
class HoverHighlightView;
class SystemTrayItem;
class TrayPopupLabelButton;

namespace tray {

class NetworkStateListDetailedView
    : public NetworkDetailedView,
      public views::ButtonListener,
      public ViewClickListener,
      public ui::NetworkListDelegate,
      public base::SupportsWeakPtr<NetworkStateListDetailedView> {
 public:
  enum ListType {
    LIST_TYPE_NETWORK,
    LIST_TYPE_VPN
  };

  NetworkStateListDetailedView(SystemTrayItem* owner,
                               ListType list_type,
                               user::LoginStatus login);
  virtual ~NetworkStateListDetailedView();

  // Overridden from NetworkDetailedView:
  virtual void Init() OVERRIDE;
  virtual DetailedViewType GetViewType() const OVERRIDE;
  virtual void ManagerChanged() OVERRIDE;
  virtual void NetworkListChanged() OVERRIDE;
  virtual void NetworkServiceChanged(
      const chromeos::NetworkState* network) OVERRIDE;

 protected:
  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from ViewClickListener.
  virtual void OnViewClicked(views::View* sender) OVERRIDE;

 private:
  class InfoBubble;

  // Create UI components.
  void CreateHeaderEntry();
  void CreateHeaderButtons();
  void CreateNetworkExtra();

  // Update UI components.
  void UpdateHeaderButtons();
  void UpdateTechnologyButton(TrayPopupHeaderButton* button,
                              const chromeos::NetworkTypePattern& technology);

  void UpdateNetworkList();

  bool OrderChild(views::View* view, int index);

  void UpdateNetworkExtra();

  // Adds a settings entry when logged in, and an entry for changing proxy
  // settings otherwise.
  void CreateSettingsEntry();

  // Create and manage the network info bubble.
  void ToggleInfoBubble();
  bool ResetInfoBubble();
  void OnInfoBubbleDestroyed();
  views::View* CreateNetworkInfoView();

  // Periodically request a network scan.
  void CallRequestScan();

  // Handle toggile mobile action
  void ToggleMobile();

  // ui::NetworkListDelegate:
  virtual views::View* CreateViewForNetwork(
      const ui::NetworkInfo& info) OVERRIDE;
  virtual bool IsViewHovered(views::View* view) OVERRIDE;
  virtual chromeos::NetworkTypePattern GetNetworkTypePattern() const OVERRIDE;
  virtual void UpdateViewForNetwork(views::View* view,
                                    const ui::NetworkInfo& info) OVERRIDE;
  virtual views::Label* CreateInfoLabel() OVERRIDE;
  virtual void RelayoutScrollList() OVERRIDE;

  // Type of list (all networks or vpn)
  ListType list_type_;

  // Track login state.
  user::LoginStatus login_;

  // Child views.
  TrayPopupHeaderButton* info_icon_;
  TrayPopupHeaderButton* button_wifi_;
  TrayPopupHeaderButton* button_mobile_;
  TrayPopupLabelButton* other_wifi_;
  TrayPopupLabelButton* turn_on_wifi_;
  TrayPopupLabelButton* other_mobile_;
  TrayPopupLabelButton* other_vpn_;
  TrayPopupLabelButton* settings_;
  TrayPopupLabelButton* proxy_settings_;

  // A small bubble for displaying network info.
  views::BubbleDelegateView* info_bubble_;

  ui::NetworkListView network_list_view_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateListDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW

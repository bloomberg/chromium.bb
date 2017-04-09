// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_

#include <memory>
#include <string>

#include "ash/login_status.h"
#include "ash/system/network/network_detailed_view.h"
#include "ash/system/network/network_list_delegate.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/custom_button.h"

namespace chromeos {
class NetworkTypePattern;
}

namespace ash {
class NetworkListViewBase;
}

namespace views {
class BubbleDialogDelegateView;
}

namespace ash {
class SystemTrayItem;

namespace tray {

class NetworkStateListDetailedView
    : public NetworkDetailedView,
      public NetworkListDelegate,
      public base::SupportsWeakPtr<NetworkStateListDetailedView> {
 public:
  enum ListType { LIST_TYPE_NETWORK, LIST_TYPE_VPN };

  NetworkStateListDetailedView(SystemTrayItem* owner,
                               ListType list_type,
                               LoginStatus login);
  ~NetworkStateListDetailedView() override;

  // Overridden from NetworkDetailedView:
  void Init() override;
  DetailedViewType GetViewType() const override;
  void Update() override;

 private:
  class InfoBubble;

  // TrayDetailsView:
  void HandleViewClicked(views::View* view) override;
  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override;
  void CreateExtraTitleRowButtons() override;

  // Launches the WebUI settings in a browser and closes the system menu.
  void ShowSettings();

  // Update UI components.
  void UpdateNetworkList();
  void UpdateHeaderButtons();

  // Create and manage the network info bubble.
  void ToggleInfoBubble();
  bool ResetInfoBubble();
  void OnInfoBubbleDestroyed();
  views::View* CreateNetworkInfoView();

  // Creates the view of an extra icon appearing next to the network name
  // indicating that the network is controlled by an extension. If no extension
  // is registered for this network, returns |nullptr|.
  views::View* CreateControlledByExtensionView(const NetworkInfo& info);

  // Periodically request a network scan.
  void CallRequestScan();

  // NetworkListDelegate:
  views::View* CreateViewForNetwork(const NetworkInfo& info) override;
  chromeos::NetworkTypePattern GetNetworkTypePattern() const override;
  void UpdateViewForNetwork(views::View* view,
                            const NetworkInfo& info) override;
  views::Label* CreateInfoLabel() override;
  void OnNetworkEntryClicked(views::View* sender) override;
  void OnOtherWifiClicked() override;
  void RelayoutScrollList() override;

  // Type of list (all networks or vpn)
  ListType list_type_;

  // Track login state.
  LoginStatus login_;

  views::CustomButton* info_button_;
  views::CustomButton* settings_button_;
  views::CustomButton* proxy_settings_button_;

  // A small bubble for displaying network info.
  views::BubbleDialogDelegateView* info_bubble_;

  std::unique_ptr<NetworkListViewBase> network_list_view_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateListDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_

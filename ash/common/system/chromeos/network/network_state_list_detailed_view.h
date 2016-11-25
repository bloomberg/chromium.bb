// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_

#include <memory>
#include <string>

#include "ash/common/login_status.h"
#include "ash/common/system/chromeos/network/network_detailed_view.h"
#include "ash/common/system/chromeos/network/network_list_delegate.h"
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
class ImageButton;
}

namespace ash {
class SystemTrayItem;
class ThrobberView;
class TrayPopupHeaderButton;
class TrayPopupLabelButton;

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

  // Create UI components.
  void CreateHeaderEntry();
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

  // Sets the visibility and focusability of Network Info Button and
  // WiFi scanning indicator. This will hide Network info button and display
  // the scanning indicator when |is_scanning| is true.
  void SetScanningStateForThrobberView(bool is_scanning);

  // Create and manage the network info bubble.
  void ToggleInfoBubble();
  bool ResetInfoBubble();
  void OnInfoBubbleDestroyed();
  views::View* CreateNetworkInfoView();
  const gfx::ImageSkia* GetControlledByExtensionIcon();

  // Creates the view of an extra icon appearing next to the network name
  // indicating that the network is controlled by an extension. If no extension
  // is registered for this network, returns |nullptr|.
  views::View* CreateControlledByExtensionView(const NetworkInfo& info);

  // Periodically request a network scan.
  void CallRequestScan();

  // Handle toggile mobile action
  void ToggleMobile();

  // NetworkListDelegate:
  views::View* CreateViewForNetwork(const NetworkInfo& info) override;
  bool IsViewHovered(views::View* view) override;
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

  // Tracks the WiFi scanning state to help detect if the state has changed. Use
  // NetworkHandler::GetScanningByType() if you require the current wifi
  // scanning state.
  bool prev_wifi_scanning_state_;

  // Not used for material design.
  views::ImageButton* info_icon_;

  // Not used for material design.
  TrayPopupHeaderButton* button_wifi_;

  // Not used for material design.
  TrayPopupHeaderButton* button_mobile_;

  TrayPopupLabelButton* other_wifi_;
  TrayPopupLabelButton* turn_on_wifi_;
  TrayPopupLabelButton* other_mobile_;
  TrayPopupLabelButton* settings_;
  TrayPopupLabelButton* proxy_settings_;

  // Only used in material design.
  views::CustomButton* info_button_md_;
  views::CustomButton* settings_button_md_;
  views::CustomButton* proxy_settings_button_md_;

  // A small bubble for displaying network info.
  views::BubbleDialogDelegateView* info_bubble_;

  // WiFi scanning throbber.
  ThrobberView* scanning_throbber_;

  gfx::Image controlled_by_extension_icon_;

  std::unique_ptr<NetworkListViewBase> network_list_view_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateListDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H_

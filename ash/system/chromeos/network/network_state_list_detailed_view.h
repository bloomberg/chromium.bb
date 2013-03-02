// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW_H

#include <map>
#include <string>
#include <vector>

#include "ash/system/chromeos/network/network_detailed_view.h"
#include "ash/system/chromeos/network/network_icon.h"
#include "ash/system/chromeos/network/network_icon_animation_observer.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/user/login_status.h"
#include "base/memory/scoped_vector.h"
#include "ui/views/controls/button/button.h"

namespace chromeos {
class NetworkStateList;
}

namespace views {
class BubbleDelegateView;
}

namespace ash {
namespace internal {

class HoverHighlightView;
class TrayNetwork;

namespace tray {

struct NetworkInfo;

class NetworkStateListDetailedView : public NetworkDetailedView,
                                     public views::ButtonListener,
                                     public ViewClickListener,
                                     public network_icon::AnimationObserver {
 public:
  NetworkStateListDetailedView(TrayNetwork* tray_network,
                               user::LoginStatus login);
  virtual ~NetworkStateListDetailedView();

  // Overridden from NetworkDetailedView:
  virtual void Init() OVERRIDE;
  virtual DetailedViewType GetViewType() const OVERRIDE;
  virtual void ManagerChanged() OVERRIDE;
  virtual void NetworkListChanged() OVERRIDE;
  virtual void NetworkServiceChanged(
      const chromeos::NetworkState* network) OVERRIDE;

  // network_icon::AnimationObserver overrides
  virtual void NetworkIconChanged() OVERRIDE;

 protected:
  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE;

 private:
  typedef std::map<views::View*, std::string> NetworkMap;
  typedef std::map<std::string, HoverHighlightView*> ServicePathMap;

  // Create UI components.
  void CreateHeaderEntry();
  void CreateHeaderButtons();
  void CreateMobileAccount();
  void CreateNetworkExtra();

  // Update UI components.
  void UpdateHeaderButtons();

  void UpdateNetworks(const NetworkStateList& networks);
  void UpdateNetworkListEntries();
  bool CreateOrUpdateInfoLabel(
      int index, const string16& text, views::Label** label);
  bool UpdateNetworkChild(int index, const NetworkInfo* info);
  bool OrderChild(views::View* view, int index);
  bool UpdateNetworkListEntries(std::set<std::string>* new_service_paths);
  void UpdateMobileAccount();
  void UpdateNetworkExtra();

  // Adds a settings entry when logged in, and an entry for changing proxy
  // settings otherwise.
  void CreateSettingsEntry();

  // Create and manage the network info bubble.
  void ToggleInfoBubble();
  bool ResetInfoBubble();
  views::View* CreateNetworkInfoView();

  // Typed pointer to the owning tray item.
  TrayNetwork* tray_network_;

  // Track login state.
  user::LoginStatus login_;

  // A map of child views to their network service path.
  NetworkMap network_map_;

  // A map of network service paths to their view.
  ServicePathMap service_path_map_;

  // An owned list of network info.
  ScopedVector<NetworkInfo> network_list_;

  // Cached cellular carrier state info.
  std::string carrier_id_;
  std::string topup_url_;
  std::string setup_url_;

  // Child views.
  TrayPopupHeaderButton* info_icon_;
  TrayPopupHeaderButton* button_wifi_;
  TrayPopupHeaderButton* button_mobile_;
  views::View* view_mobile_account_;
  views::View* setup_mobile_account_;
  TrayPopupLabelButton* other_wifi_;
  TrayPopupLabelButton* turn_on_wifi_;
  TrayPopupLabelButton* other_mobile_;
  TrayPopupLabelButton* settings_;
  TrayPopupLabelButton* proxy_settings_;
  views::Label* scanning_view_;
  views::Label* no_wifi_networks_view_;
  views::Label* no_cellular_networks_view_;

  // A small bubble for displaying network info.
  views::BubbleDelegateView* info_bubble_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateListDetailedView);
};

}  // namespace tray
}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_STATE_LIST_DETAILED_VIEW

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_LIST_DETAILED_VIEW_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_LIST_DETAILED_VIEW_H

#include <string>

#include "ash/system/chromeos/network/network_list_detailed_view_base.h"
#include "ash/system/user/login_status.h"

namespace views {
class View;
}

namespace ash {

class SystemTrayItem;

namespace internal {

class TrayPopupHeaderButton;
class TrayPopupLabelButton;

namespace tray {

class NetworkListDetailedView : public NetworkListDetailedViewBase {
 public:
  NetworkListDetailedView(SystemTrayItem* owner,
                          user::LoginStatus login,
                          int header_string_id);
  virtual ~NetworkListDetailedView();

  // NetworkListDetailedViewBase
  virtual void AppendHeaderButtons() OVERRIDE;
  virtual void UpdateHeaderButtons() OVERRIDE;
  virtual void AppendNetworkEntries() OVERRIDE;
  virtual void GetAvailableNetworkList(
      std::vector<NetworkIconInfo>* list) OVERRIDE;
  virtual void RefreshNetworkScrollWithEmptyNetworkList() OVERRIDE;
  virtual void UpdateNetworkEntries() OVERRIDE;
  virtual void AppendCustomButtonsToBottomRow(views::View* bottom_row) OVERRIDE;
  virtual void UpdateNetworkExtra() OVERRIDE;
  virtual void CustomButtonPressed(views::Button* sender,
                                   const ui::Event& event) OVERRIDE;
  virtual bool CustomLinkClickedOn(views::View* sender) OVERRIDE;
  virtual bool UpdateNetworkListEntries(
      std::set<std::string>* new_service_paths) OVERRIDE;
  virtual void ClearNetworkListEntries() OVERRIDE;

 private:
  std::string carrier_id_;
  std::string topup_url_;
  std::string setup_url_;

  views::View* airplane_;
  TrayPopupHeaderButton* button_wifi_;
  TrayPopupHeaderButton* button_mobile_;
  views::View* view_mobile_account_;
  views::View* setup_mobile_account_;
  TrayPopupLabelButton* other_wifi_;
  TrayPopupLabelButton* turn_on_wifi_;
  TrayPopupLabelButton* other_mobile_;
  views::Label* scanning_view_;
  views::Label* no_wifi_networks_view_;
  views::Label* no_cellular_networks_view_;

  DISALLOW_COPY_AND_ASSIGN(NetworkListDetailedView);
};

}  // namespace tray
}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_LIST_DETAILED_VIEW_H

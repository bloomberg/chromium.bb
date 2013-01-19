// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_DETAILED_VIEW_BASE_H
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_DETAILED_VIEW_BASE_H

#include "ash/system/chromeos/network/network_detailed_view.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/user/login_status.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/view.h"

namespace views {
class BubbleDelegateView;
}

namespace ash {

struct NetworkIconInfo;

namespace internal {
namespace tray {

// Base class for the NetworkListDetailedView and VpnListDetailedView.
class NetworkListDetailedViewBase : public NetworkDetailedView,
                                    public views::ButtonListener,
                                    public ViewClickListener {
 public:
  NetworkListDetailedViewBase(SystemTrayItem* owner,
                              user::LoginStatus login,
                              int header_string_id);
  virtual ~NetworkListDetailedViewBase();

  // Overridden from NetworkDetailedView.
  virtual void Init() OVERRIDE;
  virtual NetworkDetailedView::DetailedViewType GetViewType() const OVERRIDE;
  virtual void ManagerChanged() OVERRIDE;
  virtual void NetworkListChanged(const NetworkStateList& networks) OVERRIDE;
  virtual void NetworkServiceChanged(
      const chromeos::NetworkState* network) OVERRIDE;

 protected:
  void AppendInfoButtonToHeader();
  void UpdateSettingButton();
  void ClearNetworkScrollWithEmptyNetworkList();
  void RefreshNetworkScrollWithUpdatedNetworkData();
  user::LoginStatus login() const { return login_; }
  bool IsNetworkListEmpty() const;

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE;

 private:
  typedef std::map<views::View*, std::string> NetworkMap;
  typedef std::map<std::string, HoverHighlightView*> ServicePathMap;

  virtual void AppendHeaderButtons() = 0;
  virtual void UpdateHeaderButtons() = 0;
  virtual void AppendNetworkEntries() = 0;
  virtual void GetAvailableNetworkList(std::vector<NetworkIconInfo>* list) = 0;
  virtual void RefreshNetworkScrollWithEmptyNetworkList() = 0;
  virtual void UpdateNetworkEntries() = 0;
  virtual void AppendCustomButtonsToBottomRow(views::View* bottom_row) = 0;
  virtual void UpdateNetworkExtra() = 0;
  virtual void CustomButtonPressed(views::Button* sender,
      const ui::Event& event) = 0;
  // Returns true if custom link is clicked on.
  virtual bool CustomLinkClickedOn(views::View* sender) = 0;

  void Update();
  void CreateItems();
  void AppendHeaderEntry(int header_string_id);
  void AppendNetworkExtra();
  void UpdateAvailableNetworkList();
  bool CreateOrUpdateInfoLabel(
      int index, const string16& text, views::Label** label);
  bool UpdateNetworkChild(
      int index, bool highlight, const NetworkIconInfo* info);
  bool OrderChild(views::View* view, int index);
  void RefreshNetworkList();
  // Adds a settings entry when logged in, and an entry for changing proxy
  // settings otherwise.
  void CreateSettingsEntry();
  views::View* CreateNetworkInfoView();
  void ToggleInfoBubble();
  // Returns whether an existing info-bubble was closed.
  bool ResetInfoBubble();

  user::LoginStatus login_;
  std::vector<NetworkIconInfo> network_list_;
  int header_string_id_;
  NetworkMap network_map_;
  ServicePathMap service_path_map_;
  TrayPopupHeaderButton* info_icon_;
  TrayPopupLabelButton* settings_;
  TrayPopupLabelButton* proxy_settings_;
  views::Label* scanning_view_;
  views::Label* no_wifi_networks_view_;
  views::Label* no_cellular_networks_view_;
  views::BubbleDelegateView* info_bubble_;

  DISALLOW_COPY_AND_ASSIGN(NetworkListDetailedViewBase);
};

}  // namespace tray
}  // namespace internal
}  // namespace ash

#endif  // #ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_DETAILED_VIEW_BASE_H

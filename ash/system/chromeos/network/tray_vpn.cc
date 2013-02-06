// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_vpn.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/network_list_detailed_view_base.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_more.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace internal {

namespace tray {

class VpnDefaultView : public TrayItemMore {
 public:
  VpnDefaultView(SystemTrayItem* owner, bool show_more)
      : TrayItemMore(owner, show_more) {
    Update();
  }

  virtual ~VpnDefaultView() {}

  static bool ShouldShow() {
    // Do not show VPN line in uber tray bubble if VPN is not configured.
    std::vector<NetworkIconInfo> list;
    Shell::GetInstance()->system_tray_delegate()->GetVirtualNetworks(&list);
    return list.size() != 0;
  }

  void Update() {
    NetworkIconInfo info;
    Shell::GetInstance()->system_tray_delegate()->GetVirtualNetworkIcon(&info);
    SetImage(&info.image);
    SetLabel(info.description);
    SetAccessibleName(info.description);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VpnDefaultView);
};

class VpnListDetailedView : public NetworkListDetailedViewBase {
 public:
  VpnListDetailedView(SystemTrayItem* owner,
                      user::LoginStatus login,
                      int header_string_id)
      : NetworkListDetailedViewBase(owner, login, header_string_id),
        other_vpn_(NULL),
        no_networks_view_(NULL) {
  }

  virtual ~VpnListDetailedView() {
  }

  // Overridden from NetworkListDetailedViewBase:

  virtual void AppendHeaderButtons() OVERRIDE {
    AppendInfoButtonToHeader();
  }

  virtual void UpdateHeaderButtons() OVERRIDE {
    UpdateSettingButton();
  }

  virtual void AppendNetworkEntries() OVERRIDE {
    CreateScrollableList();
  }

  virtual void GetAvailableNetworkList(
      std::vector<NetworkIconInfo>* list) OVERRIDE{
    Shell::GetInstance()->system_tray_delegate()->GetVirtualNetworks(list);
  }

  virtual void UpdateNetworkEntries() OVERRIDE {
    RefreshNetworkScrollWithUpdatedNetworkData();
  }

  virtual void AppendCustomButtonsToBottomRow(
      views::View* bottom_row) OVERRIDE {
    other_vpn_ = new TrayPopupLabelButton(
        this,
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_OTHER_VPN));
    bottom_row->AddChildView(other_vpn_);
  }

  virtual void UpdateNetworkExtra() OVERRIDE {
    if (login() == user::LOGGED_IN_LOCKED)
      return;
  }

  virtual void CustomButtonPressed(views::Button* sender,
      const ui::Event& event) OVERRIDE {
    if (sender == other_vpn_)
      ash::Shell::GetInstance()->system_tray_delegate()->ShowOtherVPN();
    else
      NOTREACHED();
  }

  virtual bool CustomLinkClickedOn(views::View* sender) OVERRIDE {
    return false;
  }

  virtual bool UpdateNetworkListEntries(
      std::set<std::string>* new_service_paths) OVERRIDE {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    bool needs_relayout = false;

    int index = 0;

    for (size_t i = 0; i < network_list().size(); ++i) {
      const NetworkIconInfo* info = &network_list()[i];
      if (UpdateNetworkChild(index++, info))
        needs_relayout = true;
      new_service_paths->insert(info->service_path);
    }

    // We shouldn't be showing this list if there are no VPNs, but it could
    // be possible to get here if a VPN were removed (e.g. by a policy) while
    // the UI was open. Better to show something than nothing.
    if (network_list().empty()) {
      if (CreateOrUpdateInfoLabel(
              index++,
              rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_NO_VPN),
              &no_networks_view_)) {
        needs_relayout = true;
      }
    }

    return needs_relayout;
  }

 private:
  TrayPopupLabelButton* other_vpn_;
  views::Label* no_networks_view_;

  DISALLOW_COPY_AND_ASSIGN(VpnListDetailedView);
};

}  // namespace tray

TrayVPN::TrayVPN(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_(NULL),
      detailed_(NULL) {
  Shell::GetInstance()->system_tray_notifier()->AddVpnObserver(this);
}

TrayVPN::~TrayVPN() {
  Shell::GetInstance()->system_tray_notifier()->RemoveVpnObserver(this);
}

views::View* TrayVPN::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayVPN::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  if (status == user::LOGGED_IN_NONE)
    return NULL;

  if (!tray::VpnDefaultView::ShouldShow())
    return NULL;

  default_ = new tray::VpnDefaultView(this, status != user::LOGGED_IN_LOCKED);
  return default_;
}

views::View* TrayVPN::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);
  detailed_ = new tray::VpnListDetailedView(
      this, status, IDS_ASH_STATUS_TRAY_VPN);
  detailed_->Init();
  return detailed_;
}

views::View* TrayVPN::CreateNotificationView(user::LoginStatus status) {
  return NULL;
}

void TrayVPN::DestroyTrayView() {
}

void TrayVPN::DestroyDefaultView() {
  default_ = NULL;
}

void TrayVPN::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayVPN::DestroyNotificationView() {
}

void TrayVPN::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayVPN::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
}

void TrayVPN::OnNetworkRefresh(const NetworkIconInfo& info) {
  if (default_) {
    default_->Update();
  } else if (tray::VpnDefaultView::ShouldShow()) {
    if (system_tray()->HasSystemBubbleType(
            SystemTrayBubble::BUBBLE_TYPE_DEFAULT)) {
      // We created the default view without a VPN view, so rebuild it.
      system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
    }
  }

  if (detailed_)
    detailed_->ManagerChanged();
}

void TrayVPN::SetNetworkMessage(NetworkTrayDelegate* delegate,
                                   MessageType message_type,
                                   NetworkType network_type,
                                   const string16& title,
                                   const string16& message,
                                   const std::vector<string16>& links) {
}

void TrayVPN::ClearNetworkMessage(MessageType message_type) {
}

void TrayVPN::OnWillToggleWifi() {
}

}  // namespace internal
}  // namespace ash

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_vpn.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/chromeos/network/network_icon_animation.h"
#include "ash/system/chromeos/network/network_list_detailed_view_base.h"
#include "ash/system/chromeos/network/network_state_list_detailed_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_popup_label_button.h"
#include "base/command_line.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using chromeos::NetworkState;
using chromeos::NetworkStateHandler;

namespace {

bool UseNewNetworkHandlers() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshDisableNewNetworkStatusArea) &&
      NetworkStateHandler::IsInitialized();
}

}

namespace ash {
namespace internal {

namespace tray {

class VpnDefaultView : public TrayItemMore,
                       public network_icon::AnimationObserver {
 public:
  VpnDefaultView(SystemTrayItem* owner, bool show_more)
      : TrayItemMore(owner, show_more) {
    Update();
    if (UseNewNetworkHandlers())
      network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  }

  virtual ~VpnDefaultView() {
    if (UseNewNetworkHandlers())
      network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }

  static bool ShouldShow() {
    // Do not show VPN line in uber tray bubble if VPN is not configured.
    if (UseNewNetworkHandlers()) {
      NetworkStateHandler* handler = NetworkStateHandler::Get();
      const NetworkState* vpn = handler->FirstNetworkByType(
          flimflam::kTypeVPN);
      return vpn != NULL;
    } else {
      std::vector<NetworkIconInfo> list;
      Shell::GetInstance()->system_tray_delegate()->GetVirtualNetworks(&list);
      return list.size() != 0;
    }
  }

  void Update() {
    if (UseNewNetworkHandlers()) {
      gfx::ImageSkia image;
      string16 label;
      GetNetworkStateHandlerImageAndLabel(&image, &label);
      SetImage(&image);
      SetLabel(label);
      SetAccessibleName(label);
    } else {
      NetworkIconInfo info;
      Shell::GetInstance()->system_tray_delegate()->
          GetVirtualNetworkIcon(&info);
      SetImage(&info.image);
      SetLabel(info.description);
      SetAccessibleName(info.description);
    }
  }

  // network_icon::AnimationObserver
  virtual void NetworkIconChanged() OVERRIDE {
    Update();
  }

 private:
  void GetNetworkStateHandlerImageAndLabel(gfx::ImageSkia* image,
                                           string16* label) {
    NetworkStateHandler* handler = NetworkStateHandler::Get();
    const NetworkState* vpn = handler->FirstNetworkByType(
        flimflam::kTypeVPN);
    if (!vpn || (vpn->connection_state() == flimflam::kStateIdle)) {
      *image = network_icon::GetImageForDisconnectedNetwork(
          network_icon::ICON_TYPE_DEFAULT_VIEW, flimflam::kTypeVPN);
      if (label) {
        *label = l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_VPN_DISCONNECTED);
      }
      return;
    }
    *image = network_icon::GetImageForNetwork(
        vpn, network_icon::ICON_TYPE_DEFAULT_VIEW);
    if (label) {
      *label = network_icon::GetLabelForNetwork(
          vpn, network_icon::ICON_TYPE_DEFAULT_VIEW);
    }
  }

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
  if (UseNewNetworkHandlers())
    network_state_observer_.reset(new TrayNetworkStateObserver(this));
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

  if (UseNewNetworkHandlers()) {
    detailed_ = new tray::NetworkStateListDetailedView(
        this, tray::NetworkStateListDetailedView::LIST_TYPE_VPN, status);
  } else {
    detailed_ = new tray::VpnListDetailedView(
        this, status, IDS_ASH_STATUS_TRAY_VPN);
  }
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

void TrayVPN::NetworkStateChanged(bool list_changed) {
  if (default_)
    default_->Update();
  if (detailed_) {
    if (list_changed)
      detailed_->NetworkListChanged();
    else
      detailed_->ManagerChanged();
  }
}

void TrayVPN::NetworkServiceChanged(const chromeos::NetworkState* network) {
  if (detailed_)
    detailed_->NetworkServiceChanged(network);
}

}  // namespace internal
}  // namespace ash

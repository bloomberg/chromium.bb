// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_vpn.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/system/chromeos/network/network_state_list_detailed_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_popup_label_button.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_strings.h"
#include "grit/ui_chromeos_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/network/network_icon.h"
#include "ui/chromeos/network/network_icon_animation.h"

using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {

class VpnDefaultView : public TrayItemMore,
                       public ui::network_icon::AnimationObserver {
 public:
  VpnDefaultView(SystemTrayItem* owner, bool show_more)
      : TrayItemMore(owner, show_more) {
    Update();
  }

  virtual ~VpnDefaultView() {
    ui::network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }

  static bool ShouldShow() {
    // Do not show VPN line in uber tray bubble if VPN is not configured.
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    const NetworkState* vpn =
        handler->FirstNetworkByType(NetworkTypePattern::VPN());
    return vpn != NULL;
  }

  void Update() {
    gfx::ImageSkia image;
    base::string16 label;
    bool animating = false;
    GetNetworkStateHandlerImageAndLabel(&image, &label, &animating);
    if (animating)
      ui::network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
    else
      ui::network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(
          this);
    SetImage(&image);
    SetLabel(label);
    SetAccessibleName(label);
  }

  // ui::network_icon::AnimationObserver
  virtual void NetworkIconChanged() OVERRIDE {
    Update();
  }

 private:
  void GetNetworkStateHandlerImageAndLabel(gfx::ImageSkia* image,
                                           base::string16* label,
                                           bool* animating) {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    const NetworkState* vpn =
        handler->FirstNetworkByType(NetworkTypePattern::VPN());
    if (!vpn || (vpn->connection_state() == shill::kStateIdle)) {
      *image = ui::network_icon::GetImageForDisconnectedNetwork(
          ui::network_icon::ICON_TYPE_DEFAULT_VIEW, shill::kTypeVPN);
      if (label) {
        *label = l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_VPN_DISCONNECTED);
      }
      *animating = false;
      return;
    }
    *animating = vpn->IsConnectingState();
    *image = ui::network_icon::GetImageForNetwork(
        vpn, ui::network_icon::ICON_TYPE_DEFAULT_VIEW);
    if (label) {
      *label = ui::network_icon::GetLabelForNetwork(
          vpn, ui::network_icon::ICON_TYPE_DEFAULT_VIEW);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(VpnDefaultView);
};

}  // namespace tray

TrayVPN::TrayVPN(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_(NULL),
      detailed_(NULL) {
  network_state_observer_.reset(new TrayNetworkStateObserver(this));
}

TrayVPN::~TrayVPN() {
}

views::View* TrayVPN::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayVPN::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  if (!chromeos::NetworkHandler::IsInitialized())
    return NULL;
  if (status == user::LOGGED_IN_NONE)
    return NULL;
  if (!tray::VpnDefaultView::ShouldShow())
    return NULL;

  bool userAddingRunning = ash::Shell::GetInstance()
                               ->session_state_delegate()
                               ->IsInSecondaryLoginScreen();

  default_ = new tray::VpnDefaultView(
      this, status != user::LOGGED_IN_LOCKED && !userAddingRunning);

  return default_;
}

views::View* TrayVPN::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);
  if (!chromeos::NetworkHandler::IsInitialized())
    return NULL;

  Shell::GetInstance()->metrics()->RecordUserMetricsAction(
      ash::UMA_STATUS_AREA_DETAILED_VPN_VIEW);
  detailed_ = new tray::NetworkStateListDetailedView(
      this, tray::NetworkStateListDetailedView::LIST_TYPE_VPN, status);
  detailed_->Init();
  return detailed_;
}

void TrayVPN::DestroyTrayView() {
}

void TrayVPN::DestroyDefaultView() {
  default_ = NULL;
}

void TrayVPN::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayVPN::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayVPN::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
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

}  // namespace ash

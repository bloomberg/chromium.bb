// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/tray_vpn.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/chromeos/network/network_icon.h"
#include "ash/common/system/chromeos/network/network_icon_animation.h"
#include "ash/common/system/chromeos/network/network_icon_animation_observer.h"
#include "ash/common/system/chromeos/network/network_state_list_detailed_view.h"
#include "ash/common/system/chromeos/network/vpn_list.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_item_more.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {

class VpnDefaultView : public TrayItemMore,
                       public network_icon::AnimationObserver {
 public:
  explicit VpnDefaultView(SystemTrayItem* owner) : TrayItemMore(owner) {}

  ~VpnDefaultView() override {
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }

  static bool ShouldShow() {
    // Show the VPN entry in the ash tray bubble if at least one third-party VPN
    // provider is installed.
    if (WmShell::Get()->vpn_list()->HaveThirdPartyVPNProviders())
      return true;

    // Also show the VPN entry if at least one VPN network is configured.
    NetworkStateHandler* const handler =
        NetworkHandler::Get()->network_state_handler();
    if (handler->FirstNetworkByType(NetworkTypePattern::VPN()))
      return true;
    return false;
  }

  void Update() {
    gfx::ImageSkia image;
    base::string16 label;
    bool animating = false;
    GetNetworkStateHandlerImageAndLabel(&image, &label, &animating);
    if (animating)
      network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
    else
      network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
    SetImage(image);
    SetLabel(label);
    SetAccessibleName(label);
  }

  // network_icon::AnimationObserver
  void NetworkIconChanged() override { Update(); }

 protected:
  // TrayItemMore:
  std::unique_ptr<TrayPopupItemStyle> HandleCreateStyle() const override {
    std::unique_ptr<TrayPopupItemStyle> style =
        TrayItemMore::HandleCreateStyle();
    style->set_color_style(
        !IsVpnEnabled()
            ? TrayPopupItemStyle::ColorStyle::DISABLED
            : IsVpnConnected() ? TrayPopupItemStyle::ColorStyle::ACTIVE
                               : TrayPopupItemStyle::ColorStyle::INACTIVE);
    return style;
  }

  void UpdateStyle() override {
    TrayItemMore::UpdateStyle();
    Update();
  }

 private:
  bool IsVpnEnabled() const {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    return handler->FirstNetworkByType(NetworkTypePattern::VPN());
  }

  bool IsVpnConnected() const {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    const NetworkState* vpn =
        handler->FirstNetworkByType(NetworkTypePattern::VPN());
    return IsVpnEnabled() &&
           (vpn->IsConnectedState() || vpn->IsConnectingState());
  }

  void GetNetworkStateHandlerImageAndLabel(gfx::ImageSkia* image,
                                           base::string16* label,
                                           bool* animating) {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    const NetworkState* vpn =
        handler->FirstNetworkByType(NetworkTypePattern::VPN());
    *image = gfx::CreateVectorIcon(
        kNetworkVpnIcon, TrayPopupItemStyle::GetIconColor(
                             vpn && vpn->IsConnectedState()
                                 ? TrayPopupItemStyle::ColorStyle::ACTIVE
                                 : TrayPopupItemStyle::ColorStyle::INACTIVE));
    if (!IsVpnConnected()) {
      if (label) {
        *label =
            l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_DISCONNECTED);
      }
      *animating = false;
      return;
    }
    *animating = vpn->IsConnectingState();
    if (label) {
      *label = network_icon::GetLabelForNetwork(
          vpn, network_icon::ICON_TYPE_DEFAULT_VIEW);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(VpnDefaultView);
};

}  // namespace tray

TrayVPN::TrayVPN(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_VPN),
      default_(nullptr),
      detailed_(nullptr) {
  network_state_observer_.reset(new TrayNetworkStateObserver(this));
}

TrayVPN::~TrayVPN() {}

views::View* TrayVPN::CreateTrayView(LoginStatus status) {
  return NULL;
}

views::View* TrayVPN::CreateDefaultView(LoginStatus status) {
  CHECK(default_ == NULL);
  if (!chromeos::NetworkHandler::IsInitialized())
    return NULL;
  if (status == LoginStatus::NOT_LOGGED_IN)
    return NULL;
  if (!tray::VpnDefaultView::ShouldShow())
    return NULL;

  const bool is_in_secondary_login_screen =
      WmShell::Get()->GetSessionStateDelegate()->IsInSecondaryLoginScreen();

  default_ = new tray::VpnDefaultView(this);
  default_->SetEnabled(status != LoginStatus::LOCKED &&
                       !is_in_secondary_login_screen);

  return default_;
}

views::View* TrayVPN::CreateDetailedView(LoginStatus status) {
  CHECK(detailed_ == NULL);
  if (!chromeos::NetworkHandler::IsInitialized())
    return NULL;

  WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_DETAILED_VPN_VIEW);
  detailed_ = new tray::NetworkStateListDetailedView(
      this, tray::NetworkStateListDetailedView::LIST_TYPE_VPN, status);
  detailed_->Init();
  return detailed_;
}

void TrayVPN::DestroyTrayView() {}

void TrayVPN::DestroyDefaultView() {
  default_ = NULL;
}

void TrayVPN::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayVPN::UpdateAfterLoginStatusChange(LoginStatus status) {}

void TrayVPN::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {}

void TrayVPN::NetworkStateChanged() {
  if (default_)
    default_->Update();
  if (detailed_)
    detailed_->Update();
}

}  // namespace ash

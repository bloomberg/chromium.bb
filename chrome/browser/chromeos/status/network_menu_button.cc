// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_button.h"

#include <algorithm>
#include <limits>

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "gfx/canvas_skia.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton

// static
const int NetworkMenuButton::kThrobDuration = 1000;

NetworkMenuButton::NetworkMenuButton(StatusAreaHost* host)
    : StatusAreaButton(this),
      NetworkMenu(),
      host_(host),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_connecting_(this)) {
  animation_connecting_.SetThrobDuration(kThrobDuration);
  animation_connecting_.SetTweenType(ui::Tween::EASE_IN_OUT);
  OnNetworkManagerChanged(CrosLibrary::Get()->GetNetworkLibrary());
  CrosLibrary::Get()->GetNetworkLibrary()->AddNetworkManagerObserver(this);
  CrosLibrary::Get()->GetNetworkLibrary()->AddCellularDataPlanObserver(this);
}

NetworkMenuButton::~NetworkMenuButton() {
  NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
  netlib->RemoveNetworkManagerObserver(this);
  netlib->RemoveObserverForAllNetworks(this);
  netlib->RemoveCellularDataPlanObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, ui::AnimationDelegate implementation:

void NetworkMenuButton::AnimationProgressed(const ui::Animation* animation) {
  if (animation == &animation_connecting_) {
    SetIcon(IconForNetworkConnecting(animation_connecting_.GetCurrentValue(),
                                     false));
    // No need to set the badge here, because it should already be set.
    SchedulePaint();
  } else {
    MenuButton::AnimationProgressed(animation);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, StatusAreaButton implementation:

void NetworkMenuButton::DrawIcon(gfx::Canvas* canvas) {
  canvas->DrawBitmapInt(IconForDisplay(icon(), badge()),
                        horizontal_padding(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::NetworkManagerObserver implementation:

void NetworkMenuButton::OnNetworkManagerChanged(NetworkLibrary* cros) {
  OnNetworkChanged(cros, cros->active_network());
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::NetworkObserver implementation:
void NetworkMenuButton::OnNetworkChanged(NetworkLibrary* cros,
                                         const Network* network) {
  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().
  SetNetworkIcon(cros, network);
  RefreshNetworkObserver(cros);
  SchedulePaint();
  UpdateMenu();
}

void NetworkMenuButton::OnCellularDataPlanChanged(NetworkLibrary* cros) {
  // Call OnNetworkManagerChanged which will update the icon.
  OnNetworkManagerChanged(cros);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkMenu implementation:

bool NetworkMenuButton::IsBrowserMode() const {
  return host_->GetScreenMode() == StatusAreaHost::kBrowserMode;
}

gfx::NativeWindow NetworkMenuButton::GetNativeWindow() const {
  return host_->GetNativeWindow();
}

void NetworkMenuButton::OpenButtonOptions() {
  host_->OpenButtonOptions(this);
}

bool NetworkMenuButton::ShouldOpenButtonOptions() const {
  return host_->ShouldOpenButtonOptions(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, private methods

void NetworkMenuButton::SetNetworkIcon(NetworkLibrary* cros,
                                       const Network* network) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  if (!cros || !CrosLibrary::Get()->EnsureLoaded()) {
    SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0));
    SetBadge(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_WARNING));
    SetTooltipText(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_STATUSBAR_NETWORK_NO_NETWORK_TOOLTIP)));
    return;
  }

  if (!cros->Connected() && !cros->Connecting()) {
    animation_connecting_.Stop();
    SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0));
    SetBadge(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED));
    SetTooltipText(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_STATUSBAR_NETWORK_NO_NETWORK_TOOLTIP)));
    return;
  }

  if (cros->wifi_connecting() || cros->cellular_connecting()) {
    // Start the connecting animation if not running.
    if (!animation_connecting_.is_animating()) {
      animation_connecting_.Reset();
      animation_connecting_.StartThrobbing(-1);
      SetIcon(IconForNetworkConnecting(0, false));
    }
    const WirelessNetwork* wireless = NULL;
    if (cros->wifi_connecting()) {
      wireless = cros->wifi_network();
      SetBadge(SkBitmap());
    } else {  // cellular_connecting
      wireless = cros->cellular_network();
      SetBadge(BadgeForNetworkTechnology(cros->cellular_network()));
    }
    SetTooltipText(UTF16ToWide(l10n_util::GetStringFUTF16(
        wireless->configuring() ? IDS_STATUSBAR_NETWORK_CONFIGURING_TOOLTIP
                                : IDS_STATUSBAR_NETWORK_CONNECTING_TOOLTIP,
        UTF8ToUTF16(wireless->name()))));
  } else {
    // Stop connecting animation since we are not connecting.
    animation_connecting_.Stop();
    // Only set the icon, if it is an active network that changed.
    if (network && network->is_active()) {
      if (network->type() == TYPE_ETHERNET) {
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_WIRED));
        SetBadge(SkBitmap());
        SetTooltipText(
            UTF16ToWide(l10n_util::GetStringFUTF16(
                IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET))));
      } else if (network->type() == TYPE_WIFI) {
        const WifiNetwork* wifi = static_cast<const WifiNetwork*>(network);
        SetIcon(IconForNetworkStrength(wifi, false));
        SetBadge(SkBitmap());
        SetTooltipText(UTF16ToWide(l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
            UTF8ToUTF16(wifi->name()))));
      } else if (network->type() == TYPE_CELLULAR) {
        const CellularNetwork* cellular =
            static_cast<const CellularNetwork*>(network);
        SetIcon(IconForNetworkStrength(cellular, false));
        SetBadge(BadgeForNetworkTechnology(cellular));
        SetTooltipText(UTF16ToWide(l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
            UTF8ToUTF16(cellular->name()))));
      }
    }
  }
}

void NetworkMenuButton::RefreshNetworkObserver(NetworkLibrary* cros) {
  const Network* network = cros->active_network();
  std::string new_network = network ? network->service_path() : std::string();
  if (active_network_ != new_network) {
    if (!active_network_.empty()) {
      cros->RemoveNetworkObserver(active_network_, this);
    }
    if (!new_network.empty()) {
      cros->AddNetworkObserver(new_network, this);
    }
    active_network_ = new_network;
  }
}

}  // namespace chromeos

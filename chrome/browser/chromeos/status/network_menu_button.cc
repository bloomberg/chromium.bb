// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_button.h"

#include <algorithm>
#include <limits>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "gfx/canvas_skia.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
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
  animation_connecting_.SetTweenType(Tween::LINEAR);
  NetworkChanged(CrosLibrary::Get()->GetNetworkLibrary());
  CrosLibrary::Get()->GetNetworkLibrary()->AddObserver(this);
}

NetworkMenuButton::~NetworkMenuButton() {
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, AnimationDelegate implementation:

void NetworkMenuButton::AnimationProgressed(const Animation* animation) {
  if (animation == &animation_connecting_) {
    // Figure out which image to draw. We want a value between 0-100.
    // 0 reperesents no signal and 100 represents full signal strength.
    int value = static_cast<int>(animation_connecting_.GetCurrentValue()*100.0);
    if (value < 0)
      value = 0;
    else if (value > 100)
      value = 100;
    SetIcon(IconForNetworkStrength(value, false));
    SchedulePaint();
  } else {
    MenuButton::AnimationProgressed(animation);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, StatusAreaButton implementation:

void NetworkMenuButton::DrawPressed(gfx::Canvas* canvas) {
  // If ethernet connected and not current connecting, then show ethernet
  // pressed icon. Otherwise, show the bars pressed icon.
  if (CrosLibrary::Get()->GetNetworkLibrary()->ethernet_connected() &&
      !animation_connecting_.is_animating())
    canvas->DrawBitmapInt(IconForDisplay(
        *ResourceBundle::GetSharedInstance().
            GetBitmapNamed(IDR_STATUSBAR_NETWORK_WIRED_PRESSED), SkBitmap()),
        0, 0);
  else
    canvas->DrawBitmapInt(IconForDisplay(
        *ResourceBundle::GetSharedInstance().
            GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS_PRESSED), SkBitmap()),
        0, 0);
}

void NetworkMenuButton::DrawIcon(gfx::Canvas* canvas) {
  canvas->DrawBitmapInt(IconForDisplay(icon(), badge()), 0, 0);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::Observer implementation:

void NetworkMenuButton::NetworkChanged(NetworkLibrary* cros) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (CrosLibrary::Get()->EnsureLoaded()) {
    if (cros->wifi_connecting() || cros->cellular_connecting()) {
      // Start the connecting animation if not running.
      if (!animation_connecting_.is_animating()) {
        animation_connecting_.Reset();
        animation_connecting_.StartThrobbing(std::numeric_limits<int>::max());
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS1));
      }
      std::string network_name = cros->wifi_connecting() ?
          cros->wifi_network().name() : cros->cellular_network().name();
      SetTooltipText(
          l10n_util::GetStringF(IDS_STATUSBAR_NETWORK_CONNECTING_TOOLTIP,
                                UTF8ToWide(network_name)));
    } else {
      // Stop connecting animation since we are not connecting.
      animation_connecting_.Stop();

      // Always show the higher priority connection first. Ethernet then wifi.
      if (cros->ethernet_connected()) {
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_WIRED));
        SetTooltipText(
            l10n_util::GetStringF(
                IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
                l10n_util::GetString(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET)));
      } else if (cros->wifi_connected()) {
        SetIcon(IconForNetworkStrength(
            cros->wifi_network().strength(), false));
        SetTooltipText(l10n_util::GetStringF(
            IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
            UTF8ToWide(cros->wifi_network().name())));
      } else if (cros->cellular_connected()) {
        const CellularNetwork& cellular = cros->cellular_network();
        if (cellular.data_left() == CellularNetwork::DATA_NONE) {
          // If no data, then we show 0 bars.
          SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0));
        } else {
          SetIcon(IconForNetworkStrength(cellular));
        }
        SetTooltipText(l10n_util::GetStringF(
            IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
            UTF8ToWide(cellular.name())));
      } else {
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0));
        SetTooltipText(l10n_util::GetString(
            IDS_STATUSBAR_NETWORK_NO_NETWORK_TOOLTIP));
      }
    }

    if (!cros->Connected() && !cros->Connecting()) {
      SetBadge(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED));
    } else if (!cros->ethernet_connected() && !cros->wifi_connected() &&
        (cros->cellular_connecting() || cros->cellular_connected())) {
      int id = IDR_STATUSBAR_NETWORK_3G;
      switch (cros->cellular_network().data_left()) {
        case CellularNetwork::DATA_NONE:
        case CellularNetwork::DATA_VERY_LOW:
          id = IDR_STATUSBAR_NETWORK_3G_VLOWDATA;
          break;
        case CellularNetwork::DATA_LOW:
          id = IDR_STATUSBAR_NETWORK_3G_LOWDATA;
          break;
        case CellularNetwork::DATA_NORMAL:
          id = IDR_STATUSBAR_NETWORK_3G;
          break;
      }
      SetBadge(*rb.GetBitmapNamed(id));
    } else {
      SetBadge(SkBitmap());
    }
  } else {
    SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0));
    SetBadge(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_WARNING));
    SetTooltipText(l10n_util::GetString(
        IDS_STATUSBAR_NETWORK_NO_NETWORK_TOOLTIP));
  }

  SchedulePaint();
  UpdateMenu();
}

void NetworkMenuButton::CellularDataPlanChanged(NetworkLibrary* cros) {
  // Call NetworkChanged which will update the icon.
  NetworkChanged(cros);
}

void NetworkMenuButton::SetBadge(const SkBitmap& badge) {
  badge_ = badge;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkMenu implementation:

bool NetworkMenuButton::IsBrowserMode() const {
  return host_->IsBrowserMode();
}

gfx::NativeWindow NetworkMenuButton::GetNativeWindow() const {
  return host_->GetNativeWindow();
}

void NetworkMenuButton::OpenButtonOptions() const {
  host_->OpenButtonOptions(this);
}

bool NetworkMenuButton::ShouldOpenButtonOptions() const {
  return host_->ShouldOpenButtonOptions(this);
}

}  // namespace chromeos

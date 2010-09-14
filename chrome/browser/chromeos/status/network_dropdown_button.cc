// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_dropdown_button.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
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
// NetworkDropdownButton

NetworkDropdownButton::NetworkDropdownButton(bool browser_mode,
                                             gfx::NativeWindow parent_window)
    : MenuButton(NULL,
                 l10n_util::GetString(IDS_STATUSBAR_NO_NETWORKS_MESSAGE),
                 this,
                 true),
      browser_mode_(browser_mode),
      parent_window_(parent_window) {
  NetworkChanged(CrosLibrary::Get()->GetNetworkLibrary());
  CrosLibrary::Get()->GetNetworkLibrary()->AddObserver(this);
}

NetworkDropdownButton::~NetworkDropdownButton() {
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, NetworkLibrary::Observer implementation:

void NetworkDropdownButton::NetworkChanged(NetworkLibrary* cros) {
  // Show network that we will actually use. It could be another network than
  // user selected. For example user selected WiFi network but we have Ethernet
  // connection and Chrome OS device will actually use Ethernet.

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (CrosLibrary::Get()->EnsureLoaded()) {
    // Always show the higher priority connection first. Ethernet then wifi.
    if (cros->ethernet_connected()) {
      SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_WIRED));
      SetText(l10n_util::GetString(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));
    } else if (cros->wifi_connected() || cros->wifi_connecting()) {
      SetIcon(IconForNetworkStrength(cros->wifi_strength(), true));
      SetText(ASCIIToWide(cros->wifi_name()));
    } else if (cros->cellular_connected() || cros->cellular_connecting()) {
      SetIcon(IconForNetworkStrength(cros->cellular_strength(), false));
      SetText(ASCIIToWide(cros->cellular_name()));
    }

    if (!cros->Connected() && !cros->Connecting()) {
      SetIcon(SkBitmap());
      SetText(l10n_util::GetString(IDS_NETWORK_SELECTION_NONE));
    }
  } else {
    SetIcon(SkBitmap());
    SetText(l10n_util::GetString(IDS_STATUSBAR_NO_NETWORKS_MESSAGE));
  }

  SchedulePaint();
}

}  // namespace chromeos

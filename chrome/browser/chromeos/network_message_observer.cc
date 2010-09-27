// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_message_observer.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace chromeos {

NetworkMessageObserver::NetworkMessageObserver(Profile* profile)
    : initialized_(false),
      notification_(profile, "network_connection.chromeos",
        IDR_NOTIFICATION_NETWORK_FAILED,
        l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE)) {
  NetworkChanged(CrosLibrary::Get()->GetNetworkLibrary());
  initialized_ = true;
}

NetworkMessageObserver::~NetworkMessageObserver() {
  notification_.Hide();
}

void NetworkMessageObserver::CreateModalPopup(views::WindowDelegate* view) {
  Browser* browser = BrowserList::GetLastActive();
  if (browser && browser->type() != Browser::TYPE_NORMAL) {
    browser = BrowserList::FindBrowserWithType(browser->profile(),
                                               Browser::TYPE_NORMAL,
                                               true);
  }
  DCHECK(browser);
  views::Window* window = views::Window::CreateChromeWindow(
      browser->window()->GetNativeHandle(), gfx::Rect(), view);
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

void NetworkMessageObserver::NetworkChanged(NetworkLibrary* obj) {
  const WifiNetworkVector& wifi_networks = obj->wifi_networks();
  const CellularNetworkVector& cellular_networks = obj->cellular_networks();

  NetworkConfigView* view = NULL;
  std::string new_failed_network;
  // Check to see if we have any newly failed wifi network.
  for (WifiNetworkVector::const_iterator it = wifi_networks.begin();
       it < wifi_networks.end(); it++) {
    const WifiNetwork& wifi = *it;
    if (wifi.failed()) {
      ServicePathWifiMap::iterator iter =
          wifi_networks_.find(wifi.service_path());
      // If the network did not previously exist, then don't do anything.
      // For example, if the user travels to a location and finds a service
      // that has previously failed, we don't want to show a notification.
      if (iter == wifi_networks_.end())
        continue;

      const WifiNetwork& wifi_old = iter->second;
      // If this network was in a failed state previously, then it's not new.
      if (wifi_old.failed())
        continue;

      // Display login box again for bad_passphrase and bad_wepkey errors.
      if (wifi.error() == ERROR_BAD_PASSPHRASE ||
          wifi.error() == ERROR_BAD_WEPKEY) {
        // The NetworkConfigView will show the appropriate error message.
        view = new NetworkConfigView(wifi, true);
        // There should only be one wifi network that failed to connect.
        // If for some reason, we have more than one failure,
        // we only display the first one. So we break here.
        break;
      }

      // If network connection failed, display a notification.
      // We only do this if we were trying to make a new connection.
      // So if a previously connected network got disconnected for any reason,
      // we don't display notification.
      if (wifi_old.connecting()) {
        new_failed_network = wifi.name();
        // Like above, there should only be one newly failed network.
        break;
      }
    }

    // If we find any network connecting, we hide any notifications.
    if (wifi.connecting()) {
      notification_.Hide();
    }
  }

  // Refresh stored networks.
  wifi_networks_.clear();
  for (WifiNetworkVector::const_iterator it = wifi_networks.begin();
       it < wifi_networks.end(); it++) {
    const WifiNetwork& wifi = *it;
    wifi_networks_[wifi.service_path()] = wifi;
  }
  cellular_networks_.clear();
  for (CellularNetworkVector::const_iterator it = cellular_networks.begin();
       it < cellular_networks.end(); it++) {
    const CellularNetwork& cellular = *it;
    cellular_networks_[cellular.service_path()] = cellular;
  }

  // Show notification if necessary.
  if (!new_failed_network.empty()) {
    // Hide if already shown to force show it in case user has closed it.
    if (notification_.visible())
      notification_.Hide();
    notification_.Show(l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE,
        ASCIIToUTF16(new_failed_network)), false, true);
  }

  // Show login box if necessary.
  if (view && initialized_)
    CreateModalPopup(view);
}

}  // namespace chromeos

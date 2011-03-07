// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_login_observer.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/window.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace chromeos {

NetworkLoginObserver::NetworkLoginObserver(NetworkLibrary* netlib) {
  RefreshStoredNetworks(netlib->wifi_networks());
  netlib->AddNetworkManagerObserver(this);
}

NetworkLoginObserver::~NetworkLoginObserver() {
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
}

void NetworkLoginObserver::CreateModalPopup(views::WindowDelegate* view) {
  Browser* browser = BrowserList::GetLastActive();
  if (browser && browser->type() != Browser::TYPE_NORMAL) {
    browser = BrowserList::FindBrowserWithType(browser->profile(),
                                               Browser::TYPE_NORMAL,
                                               true);
  }
  if (browser) {
    views::Window* window = browser::CreateViewsWindow(
        browser->window()->GetNativeHandle(), gfx::Rect(), view);
    window->SetIsAlwaysOnTop(true);
    window->Show();
  } else {
    // Browser not found, so we should be in login/oobe screen.
    BackgroundView* background_view = LoginUtils::Get()->GetBackgroundView();
    if (background_view) {
      background_view->CreateModalPopup(view);
    }
  }
}

void NetworkLoginObserver::RefreshStoredNetworks(
    const WifiNetworkVector& wifi_networks) {
  wifi_network_failures_.clear();
  for (WifiNetworkVector::const_iterator it = wifi_networks.begin();
       it < wifi_networks.end(); it++) {
    const WifiNetwork* wifi = *it;
    wifi_network_failures_[wifi->service_path()] = wifi->failed();
  }
}

void NetworkLoginObserver::OnNetworkManagerChanged(NetworkLibrary* obj) {
  const WifiNetworkVector& wifi_networks = obj->wifi_networks();

  NetworkConfigView* view = NULL;
  // Check to see if we have any newly failed wifi network.
  for (WifiNetworkVector::const_iterator it = wifi_networks.begin();
       it < wifi_networks.end(); it++) {
    WifiNetwork* wifi = *it;
    if (wifi->failed()) {
      WifiFailureMap::iterator iter =
          wifi_network_failures_.find(wifi->service_path());
      // If the network did not previously exist, then don't do anything.
      // For example, if the user travels to a location and finds a service
      // that has previously failed, we don't want to show an error.
      if (iter == wifi_network_failures_.end())
        continue;

      // If this network was in a failed state previously, then it's not new.
      if (iter->second)
        continue;

      // Display login box again for bad_passphrase and bad_wepkey errors.
      if (wifi->error() == ERROR_BAD_PASSPHRASE ||
          wifi->error() == ERROR_BAD_WEPKEY) {
        // The NetworkConfigView will show the appropriate error message.
        view = new NetworkConfigView(wifi);
        // There should only be one wifi network that failed to connect.
        // If for some reason, we have more than one failure,
        // we only display the first one. So we break here.
        break;
      }
    }
  }

  RefreshStoredNetworks(wifi_networks);

  // Show login box if necessary.
  if (view)
    CreateModalPopup(view);
}

}  // namespace chromeos

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_login_observer.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/window.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace chromeos {

NetworkLoginObserver::NetworkLoginObserver(NetworkLibrary* netlib) {
  RefreshStoredNetworks(netlib->wifi_networks(),
                        netlib->virtual_networks());
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
    const WifiNetworkVector& wifi_networks,
    const VirtualNetworkVector& virtual_networks) {
  network_failures_.clear();
  for (WifiNetworkVector::const_iterator it = wifi_networks.begin();
       it != wifi_networks.end(); it++) {
    const WifiNetwork* wifi = *it;
    network_failures_[wifi->service_path()] = wifi->failed();
  }
  for (VirtualNetworkVector::const_iterator it = virtual_networks.begin();
       it != virtual_networks.end(); it++) {
    const VirtualNetwork* vpn = *it;
    network_failures_[vpn->service_path()] = vpn->failed();
  }
}

void NetworkLoginObserver::OnNetworkManagerChanged(NetworkLibrary* cros) {
  const WifiNetworkVector& wifi_networks = cros->wifi_networks();
  const VirtualNetworkVector& virtual_networks = cros->virtual_networks();

  NetworkConfigView* view = NULL;
  // Check to see if we have any newly failed wifi network.
  for (WifiNetworkVector::const_iterator it = wifi_networks.begin();
       it != wifi_networks.end(); it++) {
    WifiNetwork* wifi = *it;
    if (wifi->failed()) {
      NetworkFailureMap::iterator iter =
          network_failures_.find(wifi->service_path());
      // If the network did not previously exist, then don't do anything.
      // For example, if the user travels to a location and finds a service
      // that has previously failed, we don't want to show an error.
      if (iter == network_failures_.end())
        continue;

      // If this network was in a failed state previously, then it's not new.
      if (iter->second)
        continue;

      // Display login dialog again for bad_passphrase and bad_wepkey errors.
      // Always re-display the login dialog for added networks that failed to
      // connect for any reason (e.g. incorrect security type was chosen).
      if (wifi->error() == ERROR_BAD_PASSPHRASE ||
          wifi->error() == ERROR_BAD_WEPKEY ||
          wifi->added()) {
        // The NetworkConfigView will show the appropriate error message.
        view = new NetworkConfigView(wifi);
        // There should only be one wifi network that failed to connect.
        // If for some reason, we have more than one failure,
        // we only display the first one. So we break here.
        break;
      }
    }
  }
  // Check to see if we have any newly failed virtual network.
  // See wifi section for detailed comments.
  if (!view) {
    for (VirtualNetworkVector::const_iterator it = virtual_networks.begin();
         it != virtual_networks.end(); it++) {
      VirtualNetwork* vpn = *it;
      if (vpn->failed()) {
        NetworkFailureMap::iterator iter =
            network_failures_.find(vpn->service_path());
        if (iter == network_failures_.end())
          continue;  // New network.
        if (iter->second)
          continue;  // Previous failure.
        if (vpn->error() == ERROR_BAD_PASSPHRASE || vpn->added()) {
          view = new NetworkConfigView(vpn);
          break;
        }
      }
    }
  }

  RefreshStoredNetworks(wifi_networks, virtual_networks);

  // Show login box if necessary.
  if (view)
    CreateModalPopup(view);
}

}  // namespace chromeos

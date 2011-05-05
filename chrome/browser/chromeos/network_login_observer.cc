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
  netlib->AddNetworkManagerObserver(this);
}

NetworkLoginObserver::~NetworkLoginObserver() {
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
}

void NetworkLoginObserver::CreateModalPopup(views::WindowDelegate* view) {
  Browser* browser = BrowserList::GetLastActive();
  if (browser && !browser->is_type_tabbed()) {
    browser = BrowserList::FindTabbedBrowser(browser->profile(), true);
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

void NetworkLoginObserver::OnNetworkManagerChanged(NetworkLibrary* cros) {
  const WifiNetworkVector& wifi_networks = cros->wifi_networks();
  const VirtualNetworkVector& virtual_networks = cros->virtual_networks();

  // Check to see if we have any newly failed wifi network.
  for (WifiNetworkVector::const_iterator it = wifi_networks.begin();
       it != wifi_networks.end(); it++) {
    WifiNetwork* wifi = *it;
    if (wifi->notify_failure()) {
      // Display login dialog again for bad_passphrase and bad_wepkey errors.
      // Always re-display the login dialog for encrypted networks that were
      // added and failed to connect for any reason.
      if (wifi->error() == ERROR_BAD_PASSPHRASE ||
          wifi->error() == ERROR_BAD_WEPKEY ||
          (wifi->encrypted() && wifi->added())) {
        CreateModalPopup(new NetworkConfigView(wifi));
        return;  // Only support one failure per notification.
      }
    }
  }
  // Check to see if we have any newly failed virtual network.
  for (VirtualNetworkVector::const_iterator it = virtual_networks.begin();
       it != virtual_networks.end(); it++) {
    VirtualNetwork* vpn = *it;
    if (vpn->notify_failure()) {
      // Display login dialog again for bad_passphrase and errors.
      // Always re-display the login dialog for newly added networks.
      if (vpn->error() == ERROR_BAD_PASSPHRASE || vpn->added()) {
        CreateModalPopup(new NetworkConfigView(vpn));
        return;  // Only support one failure per notification.
      }
    }
  }
}

}  // namespace chromeos

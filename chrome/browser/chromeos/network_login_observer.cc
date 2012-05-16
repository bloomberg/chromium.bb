// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_login_observer.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace chromeos {

NetworkLoginObserver::NetworkLoginObserver(NetworkLibrary* netlib) {
  netlib->AddNetworkManagerObserver(this);
}

NetworkLoginObserver::~NetworkLoginObserver() {
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
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
      // Always re-display for user initiated connections that fail.
      // Always re-display the login dialog for encrypted networks that were
      // added and failed to connect for any reason.
      VLOG(1) << "NotifyFailure: " << wifi->name()
              << ", error: " << wifi->error()
              << ", added: " << wifi->added();
      if (wifi->error() == ERROR_BAD_PASSPHRASE ||
          wifi->error() == ERROR_BAD_WEPKEY ||
          wifi->connection_started() ||
          (wifi->encrypted() && wifi->added())) {
        NetworkConfigView::Show(wifi, NULL);
        return;  // Only support one failure per notification.
      }
    }
  }
  // Check to see if we have any newly failed virtual network.
  for (VirtualNetworkVector::const_iterator it = virtual_networks.begin();
       it != virtual_networks.end(); it++) {
    VirtualNetwork* vpn = *it;
    if (vpn->notify_failure()) {
      VLOG(1) << "NotifyFailure: " << vpn->name()
              << ", error: " << vpn->error()
              << ", added: " << vpn->added();
      // Display login dialog for any error or newly added network.
      if (vpn->error() != ERROR_NO_ERROR || vpn->added()) {
        NetworkConfigView::Show(vpn, NULL);
        return;  // Only support one failure per notification.
      }
    }
  }
}

}  // namespace chromeos

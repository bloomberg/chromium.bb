// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_message_observer.h"

#include "app/l10n_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace chromeos {

NetworkMessageObserver::NetworkMessageObserver(Profile* profile)
  : notification_(profile, "network_connection.chromeos",
        IDR_NOTIFICATION_NETWORK_FAILED,
        l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE)) {
  // On startup, we may already have failed netorks.
  // So marked these as known failed.
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const WifiNetworkVector& wifi_networks = cros->wifi_networks();
  for (WifiNetworkVector::const_iterator it = wifi_networks.begin();
       it < wifi_networks.end(); it++) {
    const WifiNetwork& wifi = *it;
    if (wifi.failed())
      failed_networks_.insert(wifi.name());
  }
  const CellularNetworkVector& cellular_networks = cros->cellular_networks();
  for (CellularNetworkVector::const_iterator it = cellular_networks.begin();
       it < cellular_networks.end(); it++) {
    const CellularNetwork& cellular = *it;
    if (cellular.failed())
      failed_networks_.insert(cellular.name());
  }
}

NetworkMessageObserver::~NetworkMessageObserver() {
  notification_.Hide();
}

void NetworkMessageObserver::NetworkChanged(NetworkLibrary* obj) {
  std::set<std::string> failed;
  std::string new_failed_network;

  // For each call to NetworkChanged, we expect at most 1 new failed network.
  // So we just keep track of it if we find a newly failed network by comparing
  // against or stored failed networks. And we only display notification if we
  // find a newly failed network.
  const WifiNetworkVector& wifi_networks = obj->wifi_networks();
  for (WifiNetworkVector::const_iterator it = wifi_networks.begin();
       it < wifi_networks.end(); it++) {
    const WifiNetwork& wifi = *it;
    if (wifi.failed()) {
      failed.insert(wifi.name());
      if (failed_networks_.find(wifi.name())
          == failed_networks_.end())
        new_failed_network = wifi.name();
    }
  }
  const CellularNetworkVector& cellular_networks = obj->cellular_networks();
  for (CellularNetworkVector::const_iterator it = cellular_networks.begin();
       it < cellular_networks.end(); it++) {
    const CellularNetwork& cellular = *it;
    if (cellular.failed()) {
      failed.insert(cellular.name());
      if (failed_networks_.find(cellular.name())
          == failed_networks_.end())
        new_failed_network = cellular.name();
    }
  }

  failed_networks_ = failed;
  if (!new_failed_network.empty())
    notification_.Show(l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE,
        ASCIIToUTF16(new_failed_network)), true);
}

}  // namespace chromeos

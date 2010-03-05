// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_list.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "grit/generated_resources.h"

namespace chromeos {

NetworkList::NetworkList()
    : connected_network_(NULL),
      connecting_network_(NULL) {
}

NetworkList::NetworkItem* NetworkList::GetNetworkAt(int index) {
  return index >= 0 && index < static_cast<int>(networks_.size()) ?
      &networks_[index] : NULL;
}

NetworkList::NetworkItem* NetworkList::GetNetworkById(NetworkType type,
                                                      const string16& id) {
  return GetNetworkAt(GetNetworkIndexById(type, id));
}

int NetworkList::GetNetworkIndexById(NetworkType type,
                                     const string16& id) const {
  if (NETWORK_EMPTY == type || id.empty()) return -1;
  std::string network_id = UTF16ToASCII(id);
  for (size_t i = 0; i < networks_.size(); i++) {
    if (type == networks_[i].network_type) {
      switch (type) {
        case NETWORK_ETHERNET:
          // Assuming that there's only single Ethernet network.
          return i;

        case NETWORK_WIFI:
          if (network_id == networks_[i].wifi_network.ssid)
            return i;
          break;

        case NETWORK_CELLULAR:
          if (network_id == networks_[i].cellular_network.name)
            return i;
          break;

        default:
          break;
      }
    }
  }
  return -1;
}

void NetworkList::NetworkChanged(chromeos::NetworkLibrary* network_lib) {
  connected_network_ = NULL;
  connecting_network_ = NULL;
  networks_.clear();
  if (!network_lib || !network_lib->EnsureLoaded())
    return;

  if (network_lib->ethernet_connected() || network_lib->ethernet_connecting()) {
    string16 label = l10n_util::GetStringUTF16(
        IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    networks_.push_back(NetworkItem(NETWORK_ETHERNET,
                                    label,
                                    WifiNetwork(),
                                    CellularNetwork()));
    if (network_lib->ethernet_connected()) {
      connected_network_ = &networks_.back();
    } else if (network_lib->ethernet_connecting()) {
      connecting_network_ = &networks_.back();
    }
  }

  // TODO(nkostylev): Show public WiFi networks first.
  WifiNetworkVector wifi = network_lib->wifi_networks();
  for (WifiNetworkVector::const_iterator it = wifi.begin();
       it != wifi.end(); ++it) {
    networks_.push_back(NetworkItem(NETWORK_WIFI,
                                    ASCIIToUTF16(it->ssid),
                                    *it,
                                    CellularNetwork()));
    if (!connected_network_ && network_lib->wifi_ssid() == it->ssid) {
      if (network_lib->wifi_connected()) {
        connected_network_ = &networks_.back();
      } else if (network_lib->wifi_connecting()) {
        connecting_network_ = &networks_.back();
      }
    }
  }

  CellularNetworkVector cellular = network_lib->cellular_networks();
  for (CellularNetworkVector::const_iterator it = cellular.begin();
       it != cellular.end(); ++it) {
    networks_.push_back(NetworkItem(NETWORK_CELLULAR,
                                    ASCIIToUTF16(it->name),
                                    WifiNetwork(),
                                    *it));
    if (!connected_network_ && network_lib->cellular_name() == it->name) {
      if (network_lib->cellular_connected()) {
        connected_network_ = &networks_.back();
      } else if (network_lib->cellular_connecting()) {
        connecting_network_ = &networks_.back();
      }
    }
  }
}

}  // namespace chromeos

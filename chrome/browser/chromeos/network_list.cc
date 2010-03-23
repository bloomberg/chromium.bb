// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_list.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "grit/generated_resources.h"

namespace chromeos {

NetworkList::NetworkList()
    : connected_network_index_(-1),
      connecting_network_index_(-1) {
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

// Returns currently connected network if there is one.
const NetworkList::NetworkItem* NetworkList::ConnectedNetwork() const {
  if (connected_network_index_ >= 0 &&
      connected_network_index_ < static_cast<int>(networks_.size())) {
    return &networks_[connected_network_index_];
  } else {
    return NULL;
  }
}

// Returns currently connecting network if there is one.
const NetworkList::NetworkItem* NetworkList::ConnectingNetwork() const {
  if (connecting_network_index_ >= 0 &&
      connecting_network_index_ < static_cast<int>(networks_.size())) {
    return &networks_[connecting_network_index_];
  } else {
    return NULL;
  }
}

void NetworkList::NetworkChanged(chromeos::NetworkLibrary* network_lib) {
  connected_network_index_ = -1;
  connecting_network_index_ = -1;
  networks_.clear();
  // Index of the last added network item.
  int index = 0;
  if (!network_lib || !CrosLibrary::Get()->EnsureLoaded())
    return;

  if (network_lib->ethernet_connected() || network_lib->ethernet_connecting()) {
    string16 label = l10n_util::GetStringUTF16(
        IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    networks_.push_back(NetworkItem(NETWORK_ETHERNET,
                                    label,
                                    WifiNetwork(),
                                    CellularNetwork()));
    SetNetworksIndices(index++,
                       network_lib->ethernet_connected(),
                       network_lib->ethernet_connecting());
  }

  // TODO(nkostylev): Show public WiFi networks first.
  WifiNetworkVector wifi = network_lib->wifi_networks();
  for (WifiNetworkVector::const_iterator it = wifi.begin();
       it != wifi.end(); ++it, ++index) {
    networks_.push_back(NetworkItem(NETWORK_WIFI,
                                    ASCIIToUTF16(it->ssid),
                                    *it,
                                    CellularNetwork()));
    if (network_lib->wifi_ssid() == it->ssid) {
      SetNetworksIndices(index,
                         network_lib->wifi_connected(),
                         network_lib->wifi_connecting());
    }
  }

  CellularNetworkVector cellular = network_lib->cellular_networks();
  for (CellularNetworkVector::const_iterator it = cellular.begin();
       it != cellular.end(); ++it, ++index) {
    networks_.push_back(NetworkItem(NETWORK_CELLULAR,
                                    ASCIIToUTF16(it->name),
                                    WifiNetwork(),
                                    *it));
    if (network_lib->cellular_name() == it->name) {
      SetNetworksIndices(index,
                         network_lib->cellular_connected(),
                         network_lib->cellular_connected());
    }
  }
}

void NetworkList::SetNetworksIndices(int index,
                                     bool connected,
                                     bool connecting) {
  if (connected_network_index_  != -1 ||
      connecting_network_index_ != -1 ||
      index < 0 || index >= static_cast<int>(networks_.size()))
    return;

  if (connected) {
    connected_network_index_ = index;
  } else if (connecting) {
    connecting_network_index_ = index;
  }
}

}  // namespace chromeos

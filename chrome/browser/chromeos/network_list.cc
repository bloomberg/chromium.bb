// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_list.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "grit/generated_resources.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkList, public:

NetworkList::NetworkList() {
}

NetworkList::NetworkItem* NetworkList::GetNetworkAt(int index) {
  return index >= 0 && index < static_cast<int>(networks_.size()) ?
      &networks_[index] : NULL;
}

const NetworkList::NetworkItem* NetworkList::GetNetworkById(
    NetworkType type, const string16& id) {
  return GetNetworkAt(GetNetworkIndexById(type, id));
}

int NetworkList::GetNetworkIndexById(NetworkType type,
                                     const string16& id) const {
  if (type == NETWORK_EMPTY)
    return -1;
  std::string network_id = UTF16ToASCII(id);
  for (size_t i = 0; i < networks_.size(); ++i) {
    if (IsSameNetwork(&networks_[i], type, network_id))
      return i;
  }
  return -1;
}

bool NetworkList::IsNetworkConnected(NetworkType type,
                                     const string16& id) const {
  return IsInNetworkList(connected_networks_, type, id);
}

bool NetworkList::IsNetworkConnecting(NetworkType type,
                                      const string16& id) const {
  return IsInNetworkList(connecting_networks_, type, id);
}

void NetworkList::NetworkChanged(chromeos::NetworkLibrary* network_lib) {
  networks_.clear();
  connected_networks_.clear();
  connecting_networks_.clear();
  // Index of the last added network item.
  size_t index = 0;
  if (!network_lib || !CrosLibrary::Get()->EnsureLoaded())
    return;

  bool ethernet_connected = network_lib->ethernet_connected();
  bool ethernet_connecting = network_lib->ethernet_connecting();
  if (ethernet_connected || ethernet_connecting) {
    string16 label = l10n_util::GetStringUTF16(
        IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    networks_.push_back(NetworkItem(NETWORK_ETHERNET,
                                    label,
                                    WifiNetwork(),
                                    CellularNetwork()));
    AddNetworkIndexToList(index++, ethernet_connected, ethernet_connecting);
  }

  // TODO(nkostylev): Show public WiFi networks first.
  WifiNetworkVector wifi = network_lib->wifi_networks();
  for (WifiNetworkVector::const_iterator it = wifi.begin();
       it != wifi.end(); ++it, ++index) {
    networks_.push_back(NetworkItem(NETWORK_WIFI,
                                    ASCIIToUTF16(it->name()),
                                    *it,
                                    CellularNetwork()));
    if (network_lib->wifi_name() == it->name()) {
      AddNetworkIndexToList(index,
                            network_lib->wifi_connected(),
                            network_lib->wifi_connecting());
    }
  }

  CellularNetworkVector cellular = network_lib->cellular_networks();
  for (CellularNetworkVector::const_iterator it = cellular.begin();
       it != cellular.end(); ++it, ++index) {
    networks_.push_back(NetworkItem(NETWORK_CELLULAR,
                                    ASCIIToUTF16(it->name()),
                                    WifiNetwork(),
                                    *it));
    if (network_lib->cellular_name() == it->name()) {
      AddNetworkIndexToList(index,
                            network_lib->cellular_connected(),
                            network_lib->cellular_connecting());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkList, private:

bool NetworkList::IsInNetworkList(const NetworkIndexVector& list,
                                  NetworkType type,
                                  const string16& id) const {
  if (type == NETWORK_EMPTY)
    return false;
  std::string network_id = UTF16ToASCII(id);
  for (size_t i = 0; i < list.size(); ++i) {
    if (IsSameNetwork(&networks_[list[i]], type, network_id))
      return true;
  }
  return false;
}

bool NetworkList::IsSameNetwork(const NetworkList::NetworkItem* network,
                                NetworkType type,
                                const std::string& id) const {
  if (type != network->network_type)
    return false;
  switch (type) {
    case NETWORK_ETHERNET:
      // Assuming that there's only single Ethernet network.
      return true;
    case NETWORK_WIFI:
      return id == network->wifi_network.name();
      break;
    case NETWORK_CELLULAR:
      return id == network->cellular_network.name();
      break;
    default:
      return false;
      break;
  }
}

void NetworkList::AddNetworkIndexToList(size_t index,
                                        bool connected,
                                        bool connecting) {
  if (connected) {
    connected_networks_.push_back(index);
  } else if (connecting) {
    connecting_networks_.push_back(index);
  }
}

}  // namespace chromeos

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_LIST_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_LIST_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

// Represents list of currently available networks (Ethernet, Cellular, WiFi).
// TODO(nkostylev): Refactor network list which is also represented in
// NetworkMenuButton, InternetPageView.
class NetworkList  {
 public:
  enum NetworkType {
    NETWORK_EMPTY,      // Non-initialized network item.
    NETWORK_ETHERNET,
    NETWORK_CELLULAR,
    NETWORK_WIFI,
  };

  struct NetworkItem {
    NetworkItem()
        : network_type(NETWORK_EMPTY),
          connected(false) {}
    NetworkItem(NetworkType network_type,
                const string16& label)
        : network_type(network_type),
          label(label) {}
    NetworkItem(NetworkType network_type,
                string16 label,
                WifiNetwork wifi_network,
                CellularNetwork cellular_network)
        : network_type(network_type),
          label(label),
          wifi_network(wifi_network),
          cellular_network(cellular_network),
          connected(false) {}

    NetworkType network_type;
    string16 label;      // string representation of the network (shown in UI).
    WifiNetwork wifi_network;
    CellularNetwork cellular_network;
    bool connected;
  };

  NetworkList();
  virtual ~NetworkList() {}

  // True if network list is empty.
  bool IsEmpty() const {
    return networks_.empty();
  }

  // Returns true, if specified network is currently connected.
  bool IsNetworkConnected(NetworkType type, const string16& id) const;

  // Returns true, if specified network is currently connected.
  bool IsNetworkConnecting(NetworkType type, const string16& id) const;

  // Returns network by it's type and ssid (Wifi) or id (Cellular).
  // If network is not available NULL is returned.
  const NetworkList::NetworkItem* GetNetworkById(NetworkType type,
                                                 const string16& id);

  // Returns network index by it's type and ssid (Wifi) or id (Cellular).
  // If network is not available -1 is returned.
  int GetNetworkIndexById(NetworkType type, const string16& id) const;

  // Returns number of networks.
  size_t GetNetworkCount() const {
    return networks_.size();
  }

  // Returns network by index.
  NetworkList::NetworkItem* GetNetworkAt(int index);

  // Callback from NetworkLibrary.
  void NetworkChanged(chromeos::NetworkLibrary* network_lib);

 private:
  typedef std::vector<NetworkItem> NetworkItemVector;
  typedef std::vector<size_t> NetworkIndexVector;

  // Returns true if the specified network is in the list.
  bool IsInNetworkList(const NetworkIndexVector& list,
                       NetworkType type,
                       const string16& id) const;

  // Returns true if network is of the same type and id.
  bool IsSameNetwork(const NetworkList::NetworkItem* network,
                     NetworkType type,
                     const std::string& id) const;

  // Adds network index to the corresponding connected/connecting network list.
  // |index| - network index being processed
  void AddNetworkIndexToList(size_t index, bool connected, bool connecting);

  // Cached list of all available networks with their connection states.
  NetworkItemVector networks_;

  // Connected networks indexes.
  NetworkIndexVector connected_networks_;

  // Connecting networks indexes.
  NetworkIndexVector connecting_networks_;

  DISALLOW_COPY_AND_ASSIGN(NetworkList);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_LIST_H_

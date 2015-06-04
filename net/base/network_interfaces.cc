// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces.h"

namespace net {

NetworkInterface::NetworkInterface()
    : type(NetworkChangeNotifier::CONNECTION_UNKNOWN), prefix_length(0) {
}

NetworkInterface::NetworkInterface(const std::string& name,
                                   const std::string& friendly_name,
                                   uint32_t interface_index,
                                   NetworkChangeNotifier::ConnectionType type,
                                   const IPAddressNumber& address,
                                   uint32_t prefix_length,
                                   int ip_address_attributes)
    : name(name),
      friendly_name(friendly_name),
      interface_index(interface_index),
      type(type),
      address(address),
      prefix_length(prefix_length),
      ip_address_attributes(ip_address_attributes) {
}

NetworkInterface::~NetworkInterface() {
}

ScopedWifiOptions::~ScopedWifiOptions() {
}

}  // namespace net

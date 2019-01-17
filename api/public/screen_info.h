// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_SCREEN_INFO_H_
#define API_PUBLIC_SCREEN_INFO_H_

#include <cstdint>
#include <string>

#include "base/ip_address.h"
#include "platform/api/network_interface.h"

namespace openscreen {

// This contains canonical information about a specific screen found on the
// network via our discovery mechanism (mDNS).
struct ScreenInfo {
  bool operator==(const ScreenInfo& other) const;
  bool operator!=(const ScreenInfo& other) const;

  bool Update(std::string&& friendly_name,
              platform::InterfaceIndex network_interface_index,
              const IPEndpoint& v4_endpoint,
              const IPEndpoint& v6_endpoint);

  // Identifier uniquely identifying the screen.
  std::string screen_id;

  // User visible name of the screen in UTF-8.
  std::string friendly_name;

  // The index of the network interface that the screen was discovered on.
  platform::InterfaceIndex network_interface_index =
      platform::kInvalidInterfaceIndex;

  // The network endpoints to create a new connection to the screen.
  IPEndpoint v4_endpoint;
  IPEndpoint v6_endpoint;
};

}  // namespace openscreen

#endif  // API_PUBLIC_SCREEN_INFO_H_

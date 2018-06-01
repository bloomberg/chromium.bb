// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_SCREEN_INFO_H_
#define API_PUBLIC_SCREEN_INFO_H_

#include <string>

#include "base/ip_address.h"

namespace openscreen {

struct ScreenInfo {
  bool operator==(const ScreenInfo& other) const;
  bool operator!=(const ScreenInfo& other) const;

  // Identifier uniquely identifying the screen.
  std::string screen_id;

  // User visible name of the screen in UTF-8.
  std::string friendly_name;

  // The network interface that the screen was discovered on.
  std::string network_interface;

  // The network endpoint to create a new connection to the screen.
  // At least one of these two must be set.
  IPv4Endpoint ipv4_endpoint;
  IPv6Endpoint ipv6_endpoint;
};

}  // namespace openscreen

#endif

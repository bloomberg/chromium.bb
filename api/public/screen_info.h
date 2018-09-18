// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_SCREEN_INFO_H_
#define API_PUBLIC_SCREEN_INFO_H_

#include <cstdint>
#include <string>

#include "base/ip_address.h"

namespace openscreen {

struct ScreenInfo {
  bool operator==(const ScreenInfo& other) const;
  bool operator!=(const ScreenInfo& other) const;

  bool Update(std::string&& friendly_name,
              int32_t network_interface_index,
              IPEndpoint endpoint);

  // Identifier uniquely identifying the screen.
  std::string screen_id;

  // User visible name of the screen in UTF-8.
  std::string friendly_name;

  // The index of the network interface that the screen was discovered on.
  int32_t network_interface_index;

  // The network endpoint to create a new connection to the screen.
  IPEndpoint endpoint;
};

}  // namespace openscreen

#endif  // API_PUBLIC_SCREEN_INFO_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/screen_info.h"

namespace openscreen {

bool ScreenInfo::operator==(const ScreenInfo& other) const {
  return (screen_id == other.screen_id &&
          friendly_name == other.friendly_name &&
          network_interface_index == other.network_interface_index &&
          endpoint.address == other.endpoint.address &&
          endpoint.port == other.endpoint.port);
}

bool ScreenInfo::operator!=(const ScreenInfo& other) const {
  return !(*this == other);
}

bool ScreenInfo::Update(std::string&& new_friendly_name,
                        int32_t new_network_interface_index,
                        IPEndpoint new_endpoint) {
  bool changed = (friendly_name != new_friendly_name) ||
                 (network_interface_index != new_network_interface_index) ||
                 (endpoint.address != new_endpoint.address) ||
                 (endpoint.port != new_endpoint.port);
  friendly_name = std::move(new_friendly_name);
  network_interface_index = new_network_interface_index;
  endpoint = new_endpoint;
  return changed;
}

}  // namespace openscreen

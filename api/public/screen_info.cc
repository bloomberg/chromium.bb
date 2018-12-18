// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/screen_info.h"

#include <algorithm>
#include <utility>

#include "platform/api/logging.h"

namespace openscreen {

bool ScreenInfo::operator==(const ScreenInfo& other) const {
  return (screen_id == other.screen_id &&
          friendly_name == other.friendly_name &&
          network_interface_index == other.network_interface_index &&
          v4_endpoint == other.v4_endpoint && v6_endpoint == other.v6_endpoint);
}

bool ScreenInfo::operator!=(const ScreenInfo& other) const {
  return !(*this == other);
}

bool ScreenInfo::Update(std::string&& new_friendly_name,
                        platform::InterfaceIndex new_network_interface_index,
                        const IPEndpoint& new_v4_endpoint,
                        const IPEndpoint& new_v6_endpoint) {
  OSP_DCHECK(!new_v4_endpoint.address ||
             IPAddress::Version::kV4 == new_v4_endpoint.address.version());
  OSP_DCHECK(!new_v6_endpoint.address ||
             IPAddress::Version::kV6 == new_v6_endpoint.address.version());
  bool changed = (friendly_name != new_friendly_name) ||
                 (network_interface_index != new_network_interface_index) ||
                 (v4_endpoint != new_v4_endpoint) ||
                 (v6_endpoint != new_v6_endpoint);
  friendly_name = std::move(new_friendly_name);
  network_interface_index = new_network_interface_index;
  v4_endpoint = new_v4_endpoint;
  v6_endpoint = new_v6_endpoint;
  return changed;
}

}  // namespace openscreen

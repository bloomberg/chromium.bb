// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/screen_info.h"

namespace openscreen {

bool ScreenInfo::operator==(const ScreenInfo& other) const {
  return (screen_id == other.screen_id &&
          friendly_name == other.friendly_name &&
          network_interface == other.network_interface &&
          ipv4_endpoint.address == other.ipv4_endpoint.address &&
          ipv4_endpoint.port == other.ipv4_endpoint.port &&
          ipv6_endpoint.address == other.ipv6_endpoint.address &&
          ipv6_endpoint.port == other.ipv6_endpoint.port);
}

bool ScreenInfo::operator!=(const ScreenInfo& other) const {
  return !(*this == other);
}

}  // namespace openscreen

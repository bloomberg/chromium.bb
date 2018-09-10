// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/screen_info.h"

namespace openscreen {

bool ScreenInfo::operator==(const ScreenInfo& other) const {
  return (screen_id == other.screen_id &&
          friendly_name == other.friendly_name &&
          network_interface == other.network_interface &&
          endpoint.address == other.endpoint.address &&
          endpoint.port == other.endpoint.port);
}

bool ScreenInfo::operator!=(const ScreenInfo& other) const {
  return !(*this == other);
}

}  // namespace openscreen

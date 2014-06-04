// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/wifi/wifi_manager.h"

namespace local_discovery {

namespace wifi {

WifiCredentials WifiCredentials::FromPSK(const std::string& psk) {
  WifiCredentials return_value;
  return_value.psk = psk;
  return return_value;
}

}  // namespace wifi

}  // namespace local_discovery

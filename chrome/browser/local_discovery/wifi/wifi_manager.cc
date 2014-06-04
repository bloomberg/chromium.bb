// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/wifi/wifi_manager.h"

namespace local_discovery {

namespace wifi {

namespace {

WifiManagerFactory* g_factory = NULL;

}  // namespace

scoped_ptr<WifiManager> WifiManager::Create() {
  if (g_factory) {
    return g_factory->CreateWifiManager();
  }

  return CreateDefault();
}

void WifiManager::SetFactory(WifiManagerFactory* factory) {
  g_factory = factory;
}

WifiCredentials WifiCredentials::FromPSK(const std::string& psk) {
  WifiCredentials return_value;
  return_value.psk = psk;
  return return_value;
}

}  // namespace wifi

}  // namespace local_discovery

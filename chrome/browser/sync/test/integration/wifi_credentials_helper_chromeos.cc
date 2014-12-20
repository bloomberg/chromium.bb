// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/wifi_credentials_helper_chromeos.h"

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/wifi_sync/network_state_helper_chromeos.h"
#include "components/wifi_sync/wifi_security_class.h"

using WifiCredentialSet = wifi_sync::WifiCredential::CredentialSet;

namespace wifi_credentials_helper {

namespace {

const char kProfilePrefix[] = "/profile/";

std::string ChromeOsUserHashForBrowserContext(
    const content::BrowserContext& context) {
  return context.GetPath().BaseName().value();
}

std::string ShillProfilePathForBrowserContext(
    const content::BrowserContext& context) {
  return kProfilePrefix + ChromeOsUserHashForBrowserContext(context);
}

chromeos::NetworkStateHandler* GetNetworkStateHandler() {
  DCHECK(chromeos::NetworkHandler::Get()->network_state_handler());
  return chromeos::NetworkHandler::Get()->network_state_handler();
}

}  // namespace

WifiCredentialSet GetWifiCredentialsForProfileChromeOs(const Profile* profile) {
  DCHECK(profile);
  return wifi_sync::GetWifiCredentialsForShillProfile(
      GetNetworkStateHandler(), ShillProfilePathForBrowserContext(*profile));
}

}  // namespace wifi_credentials_helper

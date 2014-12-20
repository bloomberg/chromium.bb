// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WIFI_CREDENTIALS_HELPER_CHROMEOS_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WIFI_CREDENTIALS_HELPER_CHROMEOS_H_

#include "components/wifi_sync/wifi_credential.h"

class Profile;

namespace wifi_credentials_helper {

// Returns the ChromeOS WiFi credentials associated with |profile|.
wifi_sync::WifiCredential::CredentialSet GetWifiCredentialsForProfileChromeOs(
    const Profile* profile);

}  // namespace wifi_credentials_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WIFI_CREDENTIALS_HELPER_CHROMEOS_H_

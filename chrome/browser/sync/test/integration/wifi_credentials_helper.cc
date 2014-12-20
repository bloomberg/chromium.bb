// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/wifi_credentials_helper.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/wifi_sync/wifi_credential.h"
#include "components/wifi_sync/wifi_security_class.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/sync/test/integration/wifi_credentials_helper_chromeos.h"
#endif

using wifi_sync::WifiCredential;
using sync_datatype_helper::test;

using WifiCredentialSet = wifi_sync::WifiCredential::CredentialSet;

namespace wifi_credentials_helper {

namespace {

WifiCredentialSet GetWifiCredentialsForProfile(const Profile* profile) {
#if defined(OS_CHROMEOS)
  return GetWifiCredentialsForProfileChromeOs(profile);
#else
  NOTIMPLEMENTED();
  return WifiCredential::MakeSet();
#endif
}

bool CredentialsMatch(const WifiCredentialSet& a_credentials,
                      const WifiCredentialSet& b_credentials) {
  if (a_credentials.size() != b_credentials.size()) {
    LOG(ERROR) << "CredentialSets a and b do not match in size: "
               << a_credentials.size()
               << " vs " << b_credentials.size() << " respectively.";
    return false;
  }

  for (const auto &credential : a_credentials) {
    if (b_credentials.find(credential) == b_credentials.end()) {
      LOG(ERROR)
          << "Network from a not found in b. "
          << "SSID (hex): "
          << base::HexEncode(credential.ssid().data(),
                             credential.ssid().size()).c_str()
          << " SecurityClass: " << credential.security_class()
          << " Passphrase: " << credential.passphrase();
      return false;
    }
  }

  return true;
}

}  // namespace

bool VerifierIsEmpty() {
  return GetWifiCredentialsForProfile(test()->verifier()).empty();
}

bool ProfileMatchesVerifier(int profile_index) {
  WifiCredentialSet verifier_credentials =
      GetWifiCredentialsForProfile(test()->verifier());
  WifiCredentialSet other_credentials =
      GetWifiCredentialsForProfile(test()->GetProfile(profile_index));
  return CredentialsMatch(verifier_credentials, other_credentials);
}

bool AllProfilesMatch() {
  if (test()->use_verifier() && !ProfileMatchesVerifier(0)) {
    LOG(ERROR) << "Profile 0 does not match verifier.";
    return false;
  }

  WifiCredentialSet profile0_credentials =
      GetWifiCredentialsForProfile(test()->GetProfile(0));
  for (int i = 1; i < test()->num_clients(); ++i) {
    WifiCredentialSet other_profile_credentials =
        GetWifiCredentialsForProfile(test()->GetProfile(i));
    if (!CredentialsMatch(profile0_credentials, other_profile_credentials)) {
      LOG(ERROR) << "Profile " << i << " " << "does not match with profile 0.";
      return false;
    }
  }
  return true;
}

}  // namespace wifi_credentials_helper

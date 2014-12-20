// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_H_
#define COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_H_

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "components/wifi_sync/wifi_security_class.h"

namespace wifi_sync {

// A container to hold the information required to locate and connect
// to a WiFi network.
class WifiCredential final {  // final because the class is copyable
 public:
  using SsidBytes = std::vector<uint8_t>;
  using CredentialSet = std::set<
      WifiCredential,
      bool(*)(const WifiCredential&a, const WifiCredential& b)>;

  // Constructs a credential with the given |ssid|, |security_class|,
  // and |passphrase|. No assumptions are made about the input
  // encoding of |ssid|. The passphrase must be valid UTF-8.
  WifiCredential(const SsidBytes& ssid,
                 WifiSecurityClass security_class,
                 const std::string& passphrase);
  ~WifiCredential();

  const SsidBytes& ssid() const { return ssid_; }
  WifiSecurityClass security_class() const { return security_class_; }
  const std::string& passphrase() const { return passphrase_; }

  // Returns a string representation of the credential, for debugging
  // purposes. The string will not include the credential's passphrase.
  std::string ToString() const;

  // Returns true if credential |a| comes before credential |b|.
  static bool IsLessThan(const WifiCredential& a, const WifiCredential& b);

  // Returns an empty set of WifiCredentials, with the IsLessThan
  // ordering function plumbed in.
  static CredentialSet MakeSet();

 private:
  // The WiFi network's SSID.
  const SsidBytes ssid_;
  // The WiFi network's security class (e.g. WEP, PSK).
  const WifiSecurityClass security_class_;
  // The passphrase for connecting to the network.
  const std::string passphrase_;
};

}  // namespace wifi_sync

#endif  // COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wifi_sync/wifi_credential.h"

#include <limits>
#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace wifi_sync {

WifiCredential::WifiCredential(
    const std::vector<unsigned char>& ssid,
    WifiSecurityClass security_class,
    const std::string& passphrase)
    : ssid_(ssid),
      security_class_(security_class),
      passphrase_(passphrase) {
}

WifiCredential::~WifiCredential() {
}

std::string WifiCredential::ToString() const {
  return base::StringPrintf(
      "[SSID (hex): %s, SecurityClass: %d]",
      base::HexEncode(&ssid_.front(), ssid_.size()).c_str(),
      security_class_);  // Passphrase deliberately omitted.
}

// static
bool WifiCredential::IsLessThan(
    const WifiCredential& a, const WifiCredential& b) {
  return a.ssid_ < b.ssid_ ||
      a.security_class_< b.security_class_ ||
      a.passphrase_ < b.passphrase_;
}

// static
WifiCredential::CredentialSet WifiCredential::MakeSet() {
  return CredentialSet(WifiCredential::IsLessThan);
}

}  // namespace wifi_sync

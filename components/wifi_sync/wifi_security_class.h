// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_SYNC_WIFI_SECURITY_CLASS_H_
#define COMPONENTS_WIFI_SYNC_WIFI_SECURITY_CLASS_H_

#if defined(OS_CHROMEOS)
#include <string>
#endif

namespace wifi_sync {

enum WifiSecurityClass {
  SECURITY_CLASS_INVALID,
  SECURITY_CLASS_NONE,
  SECURITY_CLASS_WEP,
  SECURITY_CLASS_PSK,     // WPA-PSK or RSN-PSK
  SECURITY_CLASS_802_1X,  // WPA-Enterprise or RSN-Enterprise
};

#if defined(OS_CHROMEOS)
// Converts from a Security string presented by the ChromeOS
// connection manager ("shill") to a WifiSecurityClass enum.  Returns
// the appropriate enum value. If |security_in| is unrecognized,
// returns SECURITY_CLASS_INVALID.
WifiSecurityClass WifiSecurityClassFromShillSecurity(
    const std::string& shill_security);
#endif  // OS_CHROMEOS

}  // namespace wifi_sync

#endif  // COMPONENTS_WIFI_SYNC_WIFI_SECURITY_CLASS_H_

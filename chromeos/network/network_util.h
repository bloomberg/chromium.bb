// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_UTIL_H_
#define CHROMEOS_NETWORK_NETWORK_UTIL_H_

// This header is introduced to make it easy to switch from chromeos_network.cc
// to Chrome's own DBus code.  crosbug.com/16557
// All calls to functions in chromeos_network.h should be made through
// functions provided by this header.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/time.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// Struct to represent a SMS.
struct CHROMEOS_EXPORT SMS {
  SMS();
  ~SMS();
  base::Time timestamp;
  std::string number;
  std::string text;
  std::string smsc;  // optional; empty if not present in message.
  int32 validity;  // optional; -1 if not present in message.
  int32 msgclass;  // optional; -1 if not present in message.
};

// Struct for passing wifi access point data.
struct CHROMEOS_EXPORT WifiAccessPoint {
  WifiAccessPoint();
  ~WifiAccessPoint();
  std::string mac_address;  // The mac address of the WiFi node.
  std::string name;         // The SSID of the WiFi node.
  base::Time timestamp;     // Timestamp when this AP was detected.
  int signal_strength;      // Radio signal strength measured in dBm.
  int signal_to_noise;      // Current signal to noise ratio measured in dB.
  int channel;              // Wifi channel number.
};

typedef std::vector<WifiAccessPoint> WifiAccessPointVector;

// Describes whether there is an error and whether the error came from
// the local system or from the server implementing the connect
// method.
enum NetworkMethodErrorType {
  NETWORK_METHOD_ERROR_NONE = 0,
  NETWORK_METHOD_ERROR_LOCAL = 1,
  NETWORK_METHOD_ERROR_REMOTE = 2,
};

// Callback for methods that initiate an operation and return no data.
typedef base::Callback<void(
    const std::string& path,
    NetworkMethodErrorType error,
    const std::string& error_message)> NetworkOperationCallback;

namespace network_util {

// Converts a |prefix_length| to a netmask. (for IPv4 only)
// e.g. a |prefix_length| of 24 is converted to a netmask of "255.255.255.0".
// Invalid prefix lengths will return the empty string.
CHROMEOS_EXPORT std::string PrefixLengthToNetmask(int32 prefix_length);

// Converts a |netmask| to a prefixlen. (for IPv4 only)
// e.g. a |netmask| of 255.255.255.0 is converted to a prefixlen of 24
CHROMEOS_EXPORT int32 NetmaskToPrefixLength(const std::string& netmask);

}  // namespace network_util
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_UTIL_H_

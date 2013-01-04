// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_util.h"

#include "base/string_tokenizer.h"
#include "base/stringprintf.h"

namespace chromeos {

SMS::SMS() : validity(0), msgclass(0) {
}

SMS::~SMS() {
}

WifiAccessPoint::WifiAccessPoint()
    : signal_strength(0),
      signal_to_noise(0),
      channel(0) {
}

WifiAccessPoint::~WifiAccessPoint() {
}

namespace network_util {

std::string PrefixLengthToNetmask(int32 prefix_length) {
  std::string netmask;
  // Return the empty string for invalid inputs.
  if (prefix_length < 0 || prefix_length > 32)
    return netmask;
  for (int i = 0; i < 4; i++) {
    int remainder = 8;
    if (prefix_length >= 8) {
      prefix_length -= 8;
    } else {
      remainder = prefix_length;
      prefix_length = 0;
    }
    if (i > 0)
      netmask += ".";
    int value = remainder == 0 ? 0 :
        ((2L << (remainder - 1)) - 1) << (8 - remainder);
    netmask += StringPrintf("%d", value);
  }
  return netmask;
}

int32 NetmaskToPrefixLength(const std::string& netmask) {
  int count = 0;
  int prefix_length = 0;
  StringTokenizer t(netmask, ".");
  while (t.GetNext()) {
    // If there are more than 4 numbers, then it's invalid.
    if (count == 4)
      return -1;

    std::string token = t.token();
    // If we already found the last mask and the current one is not
    // "0" then the netmask is invalid. For example, 255.224.255.0
    if (prefix_length / 8 != count) {
      if (token != "0")
        return -1;
    } else if (token == "255") {
      prefix_length += 8;
    } else if (token == "254") {
      prefix_length += 7;
    } else if (token == "252") {
      prefix_length += 6;
    } else if (token == "248") {
      prefix_length += 5;
    } else if (token == "240") {
      prefix_length += 4;
    } else if (token == "224") {
      prefix_length += 3;
    } else if (token == "192") {
      prefix_length += 2;
    } else if (token == "128") {
      prefix_length += 1;
    } else if (token == "0") {
      prefix_length += 0;
    } else {
      // mask is not a valid number.
      return -1;
    }
    count++;
  }
  if (count < 4)
    return -1;
  return prefix_length;
}

}  // namespace network_util
}  // namespace chromeos

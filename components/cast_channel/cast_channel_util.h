// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_CHANNEL_UTIL_H_
#define COMPONENTS_CAST_CHANNEL_CAST_CHANNEL_UTIL_H_

#include "base/feature_list.h"
#include "net/base/ip_address.h"

namespace cast_channel {

// If enabled, allows Media Router to connect to Cast devices on all IP
// addresses, not just RFC1918/RFC4913 private addresses. Workaround for
// https://crbug.com/813974.
extern const base::Feature kCastAllowAllIPsFeature;

// Returns true if |ip_address| represents a valid IP address of a Cast device.
bool IsValidCastIPAddress(const net::IPAddress& ip_address);

// Similar to above, but takes a std::string as input.
bool IsValidCastIPAddressString(const std::string& ip_address_string);

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_CHANNEL_UTIL_H_

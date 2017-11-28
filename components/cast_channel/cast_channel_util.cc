// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_channel_util.h"

namespace cast_channel {

bool IsValidCastIPAddress(const net::IPAddress& ip_address) {
  // A valid Cast IP address must be private.
  return ip_address.IsReserved();
}

bool IsValidCastIPAddressString(const std::string& ip_address_string) {
  net::IPAddress ip_address;
  return ip_address.AssignFromIPLiteral(ip_address_string) &&
         IsValidCastIPAddress(ip_address);
}

}  // namespace cast_channel

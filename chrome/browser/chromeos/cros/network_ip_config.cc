// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_ip_config.h"

#include "base/logging.h"
#include "base/string_tokenizer.h"

namespace chromeos {

namespace {
#define ENUM_CASE(x) case x: return std::string(#x)
std::string IPConfigTypeAsString(IPConfigType type) {
  switch (type) {
    ENUM_CASE(IPCONFIG_TYPE_UNKNOWN);
    ENUM_CASE(IPCONFIG_TYPE_IPV4);
    ENUM_CASE(IPCONFIG_TYPE_IPV6);
    ENUM_CASE(IPCONFIG_TYPE_DHCP);
    ENUM_CASE(IPCONFIG_TYPE_BOOTP);
    ENUM_CASE(IPCONFIG_TYPE_ZEROCONF);
    ENUM_CASE(IPCONFIG_TYPE_DHCP6);
    ENUM_CASE(IPCONFIG_TYPE_PPP);
  }
  NOTREACHED() << "Unhandled enum value " << type;
  return std::string();
}
#undef ENUM_CASE
}  // namespace

NetworkIPConfig::NetworkIPConfig(
    const std::string& device_path, IPConfigType type,
    const std::string& address, const std::string& netmask,
    const std::string& gateway, const std::string& name_servers)
    : device_path(device_path),
      type(type),
      address(address),
      netmask(netmask),
      gateway(gateway),
      name_servers(name_servers) {
}

NetworkIPConfig::~NetworkIPConfig() {}

std::string NetworkIPConfig::ToString() const {
  return std::string("path: ") + device_path
      + " type: " + IPConfigTypeAsString(type)
      + " address: " + address
      + " netmask: " + netmask
      + " gateway: " + gateway
      + " name_servers: " + name_servers;
}

int32 NetworkIPConfig::GetPrefixLength() const {
  int count = 0;
  int prefixlen = 0;
  StringTokenizer t(netmask, ".");
  while (t.GetNext()) {
    // If there are more than 4 numbers, then it's invalid.
    if (count == 4) {
      return -1;
    }
    std::string token = t.token();
    // If we already found the last mask and the current one is not
    // "0" then the netmask is invalid. For example, 255.224.255.0
    if (prefixlen / 8 != count) {
      if (token != "0") {
        return -1;
      }
    } else if (token == "255") {
      prefixlen += 8;
    } else if (token == "254") {
      prefixlen += 7;
    } else if (token == "252") {
      prefixlen += 6;
    } else if (token == "248") {
      prefixlen += 5;
    } else if (token == "240") {
      prefixlen += 4;
    } else if (token == "224") {
      prefixlen += 3;
    } else if (token == "192") {
      prefixlen += 2;
    } else if (token == "128") {
      prefixlen += 1;
    } else if (token == "0") {
      prefixlen += 0;
    } else {
      // mask is not a valid number.
      return -1;
    }
    count++;
  }
  if (count < 4)
    return -1;
  return prefixlen;
}

}  // namespace chromeos

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/address_list.h"
#include "net/base/ip_address_number.h"

namespace net {

namespace {

bool IsNormalizedLocalhostTLD(const std::string& host) {
  return base::EndsWith(host, ".localhost", base::CompareCase::SENSITIVE);
}

// This function tests |host| to see if it is of any local hostname form.
// |host| is normalized before being tested and if |is_local6| is not NULL then
// it it will be set to true if the localhost name implies an IPv6 interface (
// for instance localhost6.localdomain6).
bool IsLocalHostname(base::StringPiece host, bool* is_local6) {
  std::string normalized_host = base::ToLowerASCII(host);
  // Remove any trailing '.'.
  if (!normalized_host.empty() && *normalized_host.rbegin() == '.')
    normalized_host.resize(normalized_host.size() - 1);

  if (normalized_host == "localhost6" ||
      normalized_host == "localhost6.localdomain6") {
    if (is_local6)
      *is_local6 = true;
    return true;
  }

  if (is_local6)
    *is_local6 = false;
  return normalized_host == "localhost" ||
         normalized_host == "localhost.localdomain" ||
         IsNormalizedLocalhostTLD(normalized_host);
}

}  // namespace

bool ResolveLocalHostname(base::StringPiece host,
                          uint16_t port,
                          AddressList* address_list) {
  static const unsigned char kLocalhostIPv4[] = {127, 0, 0, 1};
  static const unsigned char kLocalhostIPv6[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

  address_list->clear();

  bool is_local6;
  if (!IsLocalHostname(host, &is_local6))
    return false;

  address_list->push_back(
      IPEndPoint(IPAddressNumber(kLocalhostIPv6,
                                 kLocalhostIPv6 + arraysize(kLocalhostIPv6)),
                 port));
  if (!is_local6) {
    address_list->push_back(
        IPEndPoint(IPAddressNumber(kLocalhostIPv4,
                                   kLocalhostIPv4 + arraysize(kLocalhostIPv4)),
                   port));
  }

  return true;
}

bool IsLocalhost(base::StringPiece host) {
  if (IsLocalHostname(host, nullptr))
    return true;

  IPAddressNumber ip_number;
  if (ParseIPLiteralToNumber(host, &ip_number)) {
    size_t size = ip_number.size();
    switch (size) {
      case kIPv4AddressSize: {
        IPAddressNumber localhost_prefix;
        localhost_prefix.push_back(127);
        for (int i = 0; i < 3; ++i) {
          localhost_prefix.push_back(0);
        }
        return IPNumberMatchesPrefix(ip_number, localhost_prefix, 8);
      }

      case kIPv6AddressSize: {
        struct in6_addr sin6_addr;
        memcpy(&sin6_addr, &ip_number[0], kIPv6AddressSize);
        return !!IN6_IS_ADDR_LOOPBACK(&sin6_addr);
      }

      default:
        NOTREACHED();
    }
  }

  return false;
}

}  // namespace net

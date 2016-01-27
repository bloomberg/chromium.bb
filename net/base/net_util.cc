// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/address_list.h"
#include "net/base/ip_address_number.h"

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

namespace {

std::string NormalizeHostname(base::StringPiece host) {
  std::string result = base::ToLowerASCII(host);
  if (!result.empty() && *result.rbegin() == '.')
    result.resize(result.size() - 1);
  return result;
}

bool IsNormalizedLocalhostTLD(const std::string& host) {
  return base::EndsWith(host, ".localhost", base::CompareCase::SENSITIVE);
}

// |host| should be normalized.
bool IsLocalHostname(const std::string& host) {
  return host == "localhost" || host == "localhost.localdomain" ||
         IsNormalizedLocalhostTLD(host);
}

// |host| should be normalized.
bool IsLocal6Hostname(const std::string& host) {
  return host == "localhost6" || host == "localhost6.localdomain6";
}

}  // namespace

std::string GetHostName() {
#if defined(OS_NACL)
  NOTIMPLEMENTED();
  return std::string();
#else  // defined(OS_NACL)
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif

  // Host names are limited to 255 bytes.
  char buffer[256];
  int result = gethostname(buffer, sizeof(buffer));
  if (result != 0) {
    DVLOG(1) << "gethostname() failed with " << result;
    buffer[0] = '\0';
  }
  return std::string(buffer);
#endif  // !defined(OS_NACL)
}

bool ResolveLocalHostname(base::StringPiece host,
                          uint16_t port,
                          AddressList* address_list) {
  static const unsigned char kLocalhostIPv4[] = {127, 0, 0, 1};
  static const unsigned char kLocalhostIPv6[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

  std::string normalized_host = NormalizeHostname(host);

  address_list->clear();

  bool is_local6 = IsLocal6Hostname(normalized_host);
  if (!is_local6 && !IsLocalHostname(normalized_host))
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
  std::string normalized_host = NormalizeHostname(host);
  if (IsLocalHostname(normalized_host) || IsLocal6Hostname(normalized_host))
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

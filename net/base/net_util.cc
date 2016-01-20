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
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_canon_ip.h"

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

bool IsHostnameNonUnique(const std::string& hostname) {
  // CanonicalizeHost requires surrounding brackets to parse an IPv6 address.
  const std::string host_or_ip = hostname.find(':') != std::string::npos ?
      "[" + hostname + "]" : hostname;
  url::CanonHostInfo host_info;
  std::string canonical_name = CanonicalizeHost(host_or_ip, &host_info);

  // If canonicalization fails, then the input is truly malformed. However,
  // to avoid mis-reporting bad inputs as "non-unique", treat them as unique.
  if (canonical_name.empty())
    return false;

  // If |hostname| is an IP address, check to see if it's in an IANA-reserved
  // range.
  if (host_info.IsIPAddress()) {
    IPAddressNumber host_addr;
    if (!ParseIPLiteralToNumber(hostname.substr(host_info.out_host.begin,
                                                host_info.out_host.len),
                                &host_addr)) {
      return false;
    }
    switch (host_info.family) {
      case url::CanonHostInfo::IPV4:
      case url::CanonHostInfo::IPV6:
        return IsIPAddressReserved(host_addr);
      case url::CanonHostInfo::NEUTRAL:
      case url::CanonHostInfo::BROKEN:
        return false;
    }
  }

  // Check for a registry controlled portion of |hostname|, ignoring private
  // registries, as they already chain to ICANN-administered registries,
  // and explicitly ignoring unknown registries.
  //
  // Note: This means that as new gTLDs are introduced on the Internet, they
  // will be treated as non-unique until the registry controlled domain list
  // is updated. However, because gTLDs are expected to provide significant
  // advance notice to deprecate older versions of this code, this an
  // acceptable tradeoff.
  return 0 == registry_controlled_domains::GetRegistryLength(
                  canonical_name,
                  registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
                  registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

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

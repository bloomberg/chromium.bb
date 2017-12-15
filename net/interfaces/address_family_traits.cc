// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/interfaces/address_family_traits.h"

namespace mojo {

// static
bool EnumTraits<net::interfaces::AddressFamily, net::AddressFamily>::FromMojom(
    net::interfaces::AddressFamily address_family,
    net::AddressFamily* out) {
  using net::interfaces::AddressFamily;
  switch (address_family) {
    case AddressFamily::UNSPECIFIED:
      *out = net::ADDRESS_FAMILY_UNSPECIFIED;
      return true;
    case AddressFamily::IPV4:
      *out = net::ADDRESS_FAMILY_IPV4;
      return true;
    case AddressFamily::IPV6:
      *out = net::ADDRESS_FAMILY_IPV6;
      return true;
  }
  return false;
}

// static
net::interfaces::AddressFamily
EnumTraits<net::interfaces::AddressFamily, net::AddressFamily>::ToMojom(
    net::AddressFamily address_family) {
  using net::interfaces::AddressFamily;
  switch (address_family) {
    case net::ADDRESS_FAMILY_UNSPECIFIED:
      return AddressFamily::UNSPECIFIED;
    case net::ADDRESS_FAMILY_IPV4:
      return AddressFamily::IPV4;
    case net::ADDRESS_FAMILY_IPV6:
      return AddressFamily::IPV6;
  }
  NOTREACHED();
  return AddressFamily::UNSPECIFIED;
}

}  // namespace mojo

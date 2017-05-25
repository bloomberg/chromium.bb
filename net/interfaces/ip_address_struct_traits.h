// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_INTERFACES_IP_ADDRESS_STRUCT_TRAITS_H_
#define NET_INTERFACES_IP_ADDRESS_STRUCT_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/ip_address.h"
#include "net/interfaces/ip_address.mojom.h"

namespace mojo {
template <>
struct StructTraits<net::interfaces::IPAddressDataView, net::IPAddress> {
  static const std::vector<uint8_t> address(const net::IPAddress& ip_address) {
    // TODO(rch): avoid creating a vector here.
    return ip_address.CopyBytesToVector();
  }

  static bool Read(net::interfaces::IPAddressDataView obj, net::IPAddress* out);
};

}  // namespace mojo

#endif  // NET_INTERFACES_IP_ADDRESS_STRUCT_TRAITS_H_

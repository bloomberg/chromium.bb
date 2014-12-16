// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NET_ADDRESS_TYPE_CONVERTERS_H_
#define MOJO_SERVICES_NETWORK_NET_ADDRESS_TYPE_CONVERTERS_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/services/network/public/interfaces/net_address.mojom.h"
#include "net/base/ip_endpoint.h"

namespace mojo {

template <>
struct TypeConverter<net::IPEndPoint, NetAddressPtr> {
  static net::IPEndPoint Convert(const NetAddressPtr& obj);
};

template <>
struct TypeConverter<NetAddressPtr, net::IPEndPoint> {
  static NetAddressPtr Convert(const net::IPEndPoint& obj);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_NET_ADDRESS_TYPE_CONVERTERS_H_

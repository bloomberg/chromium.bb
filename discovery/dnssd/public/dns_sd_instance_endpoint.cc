// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/public/dns_sd_instance_endpoint.h"

#include <cctype>

#include "util/osp_logging.h"

namespace openscreen {
namespace discovery {

DnsSdInstanceEndpoint::DnsSdInstanceEndpoint(
    std::string instance_id,
    std::string service_id,
    std::string domain_id,
    DnsSdTxtRecord txt,
    IPEndpoint endpoint,
    NetworkInterfaceIndex network_interface)
    : DnsSdInstance(std::move(instance_id),
                    std::move(service_id),
                    std::move(domain_id),
                    std::move(txt),
                    endpoint.port),
      network_interface_(network_interface) {
  OSP_DCHECK(endpoint);
  if (endpoint.address.IsV4()) {
    address_v4_ = std::move(endpoint.address);
  } else if (endpoint.address.IsV6()) {
    address_v6_ = std::move(endpoint.address);
  } else {
    OSP_NOTREACHED();
  }
}

DnsSdInstanceEndpoint::DnsSdInstanceEndpoint(
    DnsSdInstance record,
    IPAddress address,
    NetworkInterfaceIndex network_interface)
    : DnsSdInstance(std::move(record)), network_interface_(network_interface) {
  OSP_DCHECK(address);
  if (address.IsV4()) {
    address_v4_ = std::move(address);
  } else if (address.IsV6()) {
    address_v6_ = std::move(address);
  } else {
    OSP_NOTREACHED();
  }
}

DnsSdInstanceEndpoint::DnsSdInstanceEndpoint(
    std::string instance_id,
    std::string service_id,
    std::string domain_id,
    DnsSdTxtRecord txt,
    IPEndpoint ipv4_endpoint,
    IPEndpoint ipv6_endpoint,
    NetworkInterfaceIndex network_interface)
    : DnsSdInstance(std::move(instance_id),
                    std::move(service_id),
                    std::move(domain_id),
                    std::move(txt),
                    ipv4_endpoint.port),
      address_v4_(std::move(ipv4_endpoint.address)),
      address_v6_(std::move(ipv6_endpoint.address)),
      network_interface_(network_interface) {
  OSP_CHECK(address_v4_);
  OSP_CHECK(address_v6_);
  OSP_CHECK(address_v4_.IsV4());
  OSP_CHECK(address_v6_.IsV6());
  OSP_CHECK_EQ(ipv4_endpoint.port, ipv6_endpoint.port);
}

DnsSdInstanceEndpoint::DnsSdInstanceEndpoint(
    DnsSdInstance instance,
    IPAddress ipv4_address,
    IPAddress ipv6_address,
    NetworkInterfaceIndex network_interface)
    : DnsSdInstance(std::move(instance)),
      address_v4_(std::move(ipv4_address)),
      address_v6_(std::move(ipv6_address)),
      network_interface_(network_interface) {
  OSP_CHECK(address_v4_);
  OSP_CHECK(address_v6_);
  OSP_CHECK(address_v4_.IsV4());
  OSP_CHECK(address_v6_.IsV6());
}

DnsSdInstanceEndpoint::~DnsSdInstanceEndpoint() = default;

IPEndpoint DnsSdInstanceEndpoint::endpoint_v4() const {
  return address_v4_ ? IPEndpoint{address_v4_, port()} : IPEndpoint{};
}

IPEndpoint DnsSdInstanceEndpoint::endpoint_v6() const {
  return address_v6_ ? IPEndpoint{address_v6_, port()} : IPEndpoint{};
}

bool operator<(const DnsSdInstanceEndpoint& lhs,
               const DnsSdInstanceEndpoint& rhs) {
  if (lhs.network_interface_ != rhs.network_interface_) {
    return lhs.network_interface_ < rhs.network_interface_;
  }

  if (lhs.address_v4_ != rhs.address_v4_) {
    return lhs.address_v4_ < rhs.address_v4_;
  }

  if (lhs.address_v6_ != rhs.address_v6_) {
    return lhs.address_v6_ < rhs.address_v6_;
  }

  return static_cast<const DnsSdInstance&>(lhs) <
         static_cast<const DnsSdInstance&>(rhs);
}

}  // namespace discovery
}  // namespace openscreen

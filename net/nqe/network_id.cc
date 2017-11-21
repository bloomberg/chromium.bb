// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_id.h"

#include <tuple>

#include "base/strings/string_number_conversions.h"

namespace {

const char kValueSeparator = ',';

// Parses |connection_type_string| as a NetworkChangeNotifier::ConnectionType.
// |connection_type_string| must contain the
// NetworkChangeNotifier::ConnectionType enum as an integer.
net::NetworkChangeNotifier::ConnectionType ConvertStringToConnectionType(
    const std::string& connection_type_string) {
  int connection_type_int =
      static_cast<int>(net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  bool connection_type_available =
      base::StringToInt(connection_type_string, &connection_type_int);

  if (!connection_type_available || connection_type_int < 0 ||
      connection_type_int >
          static_cast<int>(net::NetworkChangeNotifier::CONNECTION_LAST)) {
    DCHECK(false);
    return net::NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }
  return static_cast<net::NetworkChangeNotifier::ConnectionType>(
      connection_type_int);
}

}  // namespace

namespace net {
namespace nqe {
namespace internal {

// static
NetworkID NetworkID::FromString(const std::string& network_id) {
  size_t separator_index = network_id.find(kValueSeparator);
  DCHECK_NE(std::string::npos, separator_index);
  if (separator_index == std::string::npos) {
    return NetworkID(NetworkChangeNotifier::CONNECTION_UNKNOWN, std::string());
  }

  return NetworkID(
      ConvertStringToConnectionType(network_id.substr(separator_index + 1)),
      network_id.substr(0, separator_index));
}

NetworkID::NetworkID(NetworkChangeNotifier::ConnectionType type,
                     const std::string& id)
    : type(type), id(id) {}

NetworkID::NetworkID(const NetworkID& other) : type(other.type), id(other.id) {}

NetworkID::~NetworkID() {}

bool NetworkID::operator==(const NetworkID& other) const {
  return type == other.type && id == other.id;
}

bool NetworkID::operator!=(const NetworkID& other) const {
  return !operator==(other);
}

NetworkID& NetworkID::operator=(const NetworkID& other) {
  type = other.type;
  id = other.id;
  return *this;
}

// Overloaded to support ordered collections.
bool NetworkID::operator<(const NetworkID& other) const {
  return std::tie(type, id) < std::tie(other.type, other.id);
}

std::string NetworkID::ToString() const {
  return id + kValueSeparator + base::IntToString(static_cast<int>(type));
}

}  // namespace internal
}  // namespace nqe
}  // namespace net

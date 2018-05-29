// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ip_address.h"

namespace openscreen {

// static
bool IPv4Address::Parse(const std::string& s, IPv4Address* address) {
  if (s.size() > 0 && s[0] == '.') {
    return false;
  }
  uint16_t next_octet = 0;
  int i = 0;
  bool previous_dot = false;
  for (auto c : s) {
    if (c == '.') {
      if (previous_dot) {
        return false;
      }
      address->bytes[i++] = static_cast<uint8_t>(next_octet);
      next_octet = 0;
      previous_dot = true;
      if (i > 3) {
        return false;
      }
      continue;
    }
    previous_dot = false;
    if (!std::isdigit(c)) {
      return false;
    }
    next_octet = next_octet * 10 + (c - '0');
    if (next_octet > 255) {
      return false;
    }
  }
  if (previous_dot) {
    return false;
  }
  if (i != 3) {
    return false;
  }
  address->bytes[i] = static_cast<uint8_t>(next_octet);
  return true;
}

IPv4Address::IPv4Address() = default;
IPv4Address::IPv4Address(const std::array<uint8_t, 4>& bytes) : bytes(bytes) {}
IPv4Address::IPv4Address(const uint8_t (&b)[4])
    : bytes{{b[0], b[1], b[2], b[3]}} {}
IPv4Address::IPv4Address(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
    : bytes{{b1, b2, b3, b4}} {}
IPv4Address::IPv4Address(const IPv4Address& o) = default;

IPv4Address& IPv4Address::operator=(const IPv4Address& o) = default;

bool IPv4Address::operator==(const IPv4Address& o) const {
  return bytes == o.bytes;
}

bool IPv4Address::operator!=(const IPv4Address& o) const {
  return !(*this == o);
}

// static
bool IPv6Address::Parse(const std::string& s, IPv6Address* address) {
  if (s.size() > 1 && s[0] == ':' && s[1] != ':') {
    return false;
  }
  uint16_t next_value = 0;
  uint8_t values[16];
  int i = 0;
  int num_previous_colons = 0;
  int double_colon_index = -1;
  for (auto c : s) {
    if (c == ':') {
      ++num_previous_colons;
      if (num_previous_colons == 2) {
        if (double_colon_index != -1) {
          return false;
        }
        double_colon_index = i;
      } else if (i >= 15 || num_previous_colons > 2) {
        return false;
      } else {
        values[i++] = static_cast<uint8_t>(next_value >> 8);
        values[i++] = static_cast<uint8_t>(next_value & 0xff);
        next_value = 0;
      }
    } else {
      num_previous_colons = 0;
      uint8_t x = 0;
      if (c >= '0' && c <= '9') {
        x = c - '0';
      } else if (c >= 'a' && c <= 'f') {
        x = c - 'a' + 10;
      } else if (c >= 'A' && c <= 'F') {
        x = c - 'A' + 10;
      } else {
        return false;
      }
      if (next_value & 0xf000) {
        return false;
      } else {
        next_value = static_cast<uint16_t>(next_value * 16 + x);
      }
    }
  }
  if (num_previous_colons == 1) {
    return false;
  }
  if (i >= 15) {
    return false;
  }
  values[i++] = static_cast<uint8_t>(next_value >> 8);
  values[i] = static_cast<uint8_t>(next_value & 0xff);
  if (!((i == 15 && double_colon_index == -1) ||
        (i < 14 && double_colon_index != -1))) {
    return false;
  }
  for (int j = 15; j >= 0;) {
    if (i == double_colon_index) {
      address->bytes[j--] = values[i--];
      while (j > i)
        address->bytes[j--] = 0;
    } else {
      address->bytes[j--] = values[i--];
    }
  }
  return true;
}

IPv6Address::IPv6Address() = default;
IPv6Address::IPv6Address(const std::array<uint8_t, 16>& bytes) : bytes(bytes) {}
IPv6Address::IPv6Address(const uint8_t (&b)[16])
    : bytes{{b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10],
             b[11], b[12], b[13], b[14], b[15]}} {}
IPv6Address::IPv6Address(uint8_t b1,
                         uint8_t b2,
                         uint8_t b3,
                         uint8_t b4,
                         uint8_t b5,
                         uint8_t b6,
                         uint8_t b7,
                         uint8_t b8,
                         uint8_t b9,
                         uint8_t b10,
                         uint8_t b11,
                         uint8_t b12,
                         uint8_t b13,
                         uint8_t b14,
                         uint8_t b15,
                         uint8_t b16)
    : bytes{{b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15,
             b16}} {}
IPv6Address::IPv6Address(const IPv6Address& o) = default;

IPv6Address& IPv6Address::operator=(const IPv6Address& o) = default;

bool IPv6Address::operator==(const IPv6Address& o) const {
  return bytes == o.bytes;
}

bool IPv6Address::operator!=(const IPv6Address& o) const {
  return !(*this == o);
}

}  // namespace openscreen

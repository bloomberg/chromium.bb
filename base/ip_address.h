// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IP_ADDRESS_H_
#define BASE_IP_ADDRESS_H_

#include <array>
#include <cstdint>
#include <string>
#include <type_traits>

namespace openscreen {

// TODO(issues/9): Merge v4 and v6 IP address types.
struct IPv4Address {
 public:
  // Parses a text representation of an IPv4 address (e.g. "192.168.0.1") and
  // puts the result into |address|.  Returns true if the parsing was successful
  // and |address| was set, false otherwise.
  static bool Parse(const std::string& s, IPv4Address* address);

  IPv4Address();
  explicit IPv4Address(const std::array<uint8_t, 4>& bytes);
  explicit IPv4Address(const uint8_t (&b)[4]);
  // IPv4Address(const uint8_t*) disambiguated with
  // IPv4Address(const uint8_t (&)[4]).
  template <typename T,
            typename = typename std::enable_if<
                std::is_convertible<T, const uint8_t*>::value &&
                !std::is_convertible<T, const uint8_t (&)[4]>::value>::type>
  explicit IPv4Address(T b) : bytes{{b[0], b[1], b[2], b[3]}} {}
  IPv4Address(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4);
  IPv4Address(const IPv4Address& o);
  ~IPv4Address() = default;

  IPv4Address& operator=(const IPv4Address& o);

  bool operator==(const IPv4Address& o) const;
  bool operator!=(const IPv4Address& o) const;
  explicit operator bool() const;

  void CopyTo(uint8_t x[4]) const;

  std::array<uint8_t, 4> bytes;
};

struct IPv6Address {
 public:
  // Parses a text representation of an IPv6 address (e.g. "abcd::1234") and
  // puts the result into |address|.  Returns true if the parsing was successful
  // and |address| was set, false otherwise.
  static bool Parse(const std::string& s, IPv6Address* address);

  IPv6Address();
  explicit IPv6Address(const std::array<uint8_t, 16>& bytes);
  explicit IPv6Address(const uint8_t (&b)[16]);
  template <typename T,
            typename = typename std::enable_if<
                std::is_convertible<T, const uint8_t*>::value &&
                !std::is_convertible<T, const uint8_t (&)[16]>::value>::type>
  explicit IPv6Address(T b)
      : bytes{{b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9],
               b[10], b[11], b[12], b[13], b[14], b[15]}} {}
  IPv6Address(uint8_t b1,
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
              uint8_t b16);
  IPv6Address(const IPv6Address& o);
  ~IPv6Address() = default;

  IPv6Address& operator=(const IPv6Address& o);

  bool operator==(const IPv6Address& o) const;
  bool operator!=(const IPv6Address& o) const;
  explicit operator bool() const;

  void CopyTo(uint8_t x[16]) const;

  std::array<uint8_t, 16> bytes;
};

struct IPv4Endpoint {
 public:
  IPv4Address address;
  uint16_t port;
};

struct IPv6Endpoint {
 public:
  IPv6Address address;
  uint16_t port;
};

}  // namespace openscreen

#endif

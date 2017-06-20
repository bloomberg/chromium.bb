// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_ADDRESS_H_
#define CHROME_PROFILING_ADDRESS_H_

#include <stdint.h>

#include <functional>
#include <iosfwd>

#include "base/hash.h"

namespace profiling {

// Wrapper around an address in the instrumented process. This wrapper should
// be a zero-overhead abstraction around a 64-bit integer (so pass by value)
// that prevents getting confused between addresses in the local process and
// ones in the instrumented process.
struct Address {
  Address() : value(0) {}
  explicit Address(uint64_t v) : value(v) {}

  uint64_t value;

  bool operator<(Address other) const { return value < other.value; }
  bool operator<=(Address other) const { return value <= other.value; }
  bool operator>(Address other) const { return value > other.value; }
  bool operator>=(Address other) const { return value >= other.value; }

  bool operator==(Address other) const { return value == other.value; }
  bool operator!=(Address other) const { return value != other.value; }

  Address operator+(int64_t delta) const { return Address(value + delta); }
  Address operator+=(int64_t delta) {
    value += delta;
    return *this;
  }

  Address operator-(int64_t delta) const { return Address(value - delta); }
  Address operator-=(int64_t delta) {
    value -= delta;
    return *this;
  }

  int64_t operator-(Address a) const { return value - a.value; }
};

}  // namespace profiling

namespace std {

template <>
struct hash<profiling::Address> {
  typedef profiling::Address argument_type;
  typedef uint32_t result_type;
  result_type operator()(argument_type a) const {
    return base::Hash(reinterpret_cast<char*>(&a.value), sizeof(int64_t));
  }
};

}  // namespace std

std::ostream& operator<<(std::ostream& out, profiling::Address a);

#endif  // CHROME_PROFILING_ADDRESS_H_

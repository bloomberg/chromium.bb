// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PORTS_NAME_H_
#define MOJO_EDK_SYSTEM_PORTS_NAME_H_

#include <stdint.h>

#include <ostream>

namespace mojo {
namespace edk {
namespace ports {

struct Name {
  Name(uint64_t v1, uint64_t v2) : v1(v1), v2(v2) {}
  uint64_t v1, v2;
};

inline bool operator==(const Name& a, const Name& b) {
  return a.v1 == b.v1 && a.v2 == b.v2;
}
inline bool operator!=(const Name& a, const Name& b) {
  return !(a == b);
}
std::ostream& operator<<(std::ostream& stream, const Name& name);

struct PortName : Name {
  PortName() : Name(0, 0) {}
  PortName(uint64_t v1, uint64_t v2) : Name(v1, v2) {}
};

const PortName kInvalidPortName = {0, 0};

struct NodeName : Name {
  NodeName() : Name(0, 0) {}
  NodeName(uint64_t v1, uint64_t v2) : Name(v1, v2) {}
};

const NodeName kInvalidNodeName = {0, 0};

}  // namespace ports
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PORTS_NAME_H_

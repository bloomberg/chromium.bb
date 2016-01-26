// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PORTS_HASH_FUNCTIONS_H_
#define MOJO_EDK_SYSTEM_PORTS_HASH_FUNCTIONS_H_

#include <functional>

#include "mojo/edk/system/ports/name.h"

namespace std {

template <>
struct hash<mojo::edk::ports::PortName> {
  std::size_t operator()(const mojo::edk::ports::PortName& name) const {
    size_t h1 = hash<uint64_t>()(name.v1);
    size_t h2 = hash<uint64_t>()(name.v2);
    return h1 ^ (h2 << 1);
  }
};

template <>
struct hash<mojo::edk::ports::NodeName> {
  std::size_t operator()(const mojo::edk::ports::NodeName& name) const {
    size_t h1 = hash<uint64_t>()(name.v1);
    size_t h2 = hash<uint64_t>()(name.v2);
    return h1 ^ (h2 << 1);
  }
};

}  // namespace std

#endif  // MOJO_EDK_SYSTEM_PORTS_HASH_FUNCTIONS_H_

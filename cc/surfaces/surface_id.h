// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_ID_H_
#define CC_SURFACES_SURFACE_ID_H_

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"

namespace cc {

struct SurfaceId {
  SurfaceId() : id(0) {}
  explicit SurfaceId(uint64_t id) : id(id) {}

  bool is_null() const { return id == 0; }

  uint64_t id;
};

inline bool operator==(const SurfaceId& a, const SurfaceId& b) {
  return a.id == b.id;
}

inline bool operator!=(const SurfaceId& a, const SurfaceId& b) {
  return !(a == b);
}

inline bool operator<(const SurfaceId& a, const SurfaceId& b) {
  return a.id < b.id;
}

}  // namespace cc

namespace BASE_HASH_NAMESPACE {
#if defined(COMPILER_MSVC)
inline size_t hash_value(const cc::SurfaceId& key) {
  return hash_value(key.id);
}
#elif defined(COMPILER_GCC)
template <>
struct hash<cc::SurfaceId> {
  size_t operator()(cc::SurfaceId key) const {
    return hash<uint64_t>()(key.id);
  }
};
#else
#error define a hash function for your compiler
#endif  // COMPILER
}  // namespace BASE_HASH_NAMESPACE

#endif  // CC_SURFACES_SURFACE_ID_H_

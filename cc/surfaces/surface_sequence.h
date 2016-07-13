// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_SEQUENCE_H_
#define CC_SURFACES_SURFACE_SEQUENCE_H_

#include <stddef.h>
#include <stdint.h>

#include <tuple>

#include "base/hash.h"

namespace cc {

// A per-surface-namespace sequence number that's used to coordinate
// dependencies between frames. A sequence number may be satisfied once, and
// may be depended on once.
struct SurfaceSequence {
  SurfaceSequence() : client_id(0u), sequence(0u) {}
  SurfaceSequence(uint32_t client_id, uint32_t sequence)
      : client_id(client_id), sequence(sequence) {}
  bool is_null() const { return client_id == 0u && sequence == 0u; }

  uint32_t client_id;
  uint32_t sequence;
};

inline bool operator==(const SurfaceSequence& a, const SurfaceSequence& b) {
  return a.client_id == b.client_id && a.sequence == b.sequence;
}

inline bool operator!=(const SurfaceSequence& a, const SurfaceSequence& b) {
  return !(a == b);
}

inline bool operator<(const SurfaceSequence& a, const SurfaceSequence& b) {
  return std::tie(a.client_id, a.sequence) < std::tie(b.client_id, b.sequence);
}

struct SurfaceSequenceHash {
  size_t operator()(SurfaceSequence key) const {
    return base::HashInts(key.client_id, key.sequence);
  }
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_SEQUENCE_H_

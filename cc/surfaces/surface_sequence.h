// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_SEQUENCE_H_
#define CC_SURFACES_SURFACE_SEQUENCE_H_

namespace cc {

// A per-surface-namespace sequence number that's used to coordinate
// dependencies between frames. A sequence number may be satisfied once, and
// may be depended on once.
struct SurfaceSequence {
  SurfaceSequence() : id_namespace(0u), sequence(0u) {}
  SurfaceSequence(uint32_t id_namespace, uint32_t sequence)
      : id_namespace(id_namespace), sequence(sequence) {}

  uint32_t id_namespace;
  uint32_t sequence;
};

inline bool operator==(const SurfaceSequence& a, const SurfaceSequence& b) {
  return a.id_namespace == b.id_namespace && a.sequence == b.sequence;
}

inline bool operator!=(const SurfaceSequence& a, const SurfaceSequence& b) {
  return !(a == b);
}

inline bool operator<(const SurfaceSequence& a, const SurfaceSequence& b) {
  if (a.id_namespace != b.id_namespace)
    return a.id_namespace < b.id_namespace;
  return a.sequence < b.sequence;
}

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_SEQUENCE_H_

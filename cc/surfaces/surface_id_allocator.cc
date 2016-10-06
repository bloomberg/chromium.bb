// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_id_allocator.h"

#include <stdint.h>

#include "base/rand_util.h"

namespace cc {

SurfaceIdAllocator::SurfaceIdAllocator() : next_id_(1u) {}

SurfaceIdAllocator::~SurfaceIdAllocator() {
}

LocalFrameId SurfaceIdAllocator::GenerateId() {
  uint64_t nonce = base::RandUint64();
  LocalFrameId id(next_id_, nonce);
  next_id_++;
  return id;
}

}  // namespace cc

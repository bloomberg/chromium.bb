// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_id_allocator.h"

#include <stdint.h>

#include "base/rand_util.h"

namespace cc {

SurfaceIdAllocator::SurfaceIdAllocator(uint32_t client_id)
    : client_id_(client_id), next_id_(1u) {}

SurfaceIdAllocator::~SurfaceIdAllocator() {
}

SurfaceId SurfaceIdAllocator::GenerateId() {
  uint64_t nonce = base::RandUint64();
  SurfaceId id(client_id_, next_id_, nonce);
  next_id_++;
  return id;
}

}  // namespace cc

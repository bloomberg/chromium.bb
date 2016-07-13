// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_id_allocator.h"

#include <stdint.h>

#include "base/rand_util.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

SurfaceIdAllocator::SurfaceIdAllocator(uint32_t client_id)
    : client_id_(client_id), next_id_(1u), manager_(nullptr) {}

void SurfaceIdAllocator::RegisterSurfaceClientId(SurfaceManager* manager) {
  DCHECK(!manager_);
  manager_ = manager;
  manager_->RegisterSurfaceClientId(client_id_);
}

SurfaceIdAllocator::~SurfaceIdAllocator() {
  if (manager_)
    manager_->InvalidateSurfaceClientId(client_id_);
}

SurfaceId SurfaceIdAllocator::GenerateId() {
  uint64_t nonce = base::RandUint64();
  SurfaceId id(client_id_, next_id_, nonce);
  next_id_++;
  return id;
}

}  // namespace cc

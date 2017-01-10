// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/direct_surface_reference_factory.h"

#include <vector>

#include "cc/surfaces/surface.h"

namespace cc {

DirectSurfaceReferenceFactory::DirectSurfaceReferenceFactory(
    base::WeakPtr<SurfaceManager> manager)
    : manager_(manager) {}

DirectSurfaceReferenceFactory::~DirectSurfaceReferenceFactory() = default;
void DirectSurfaceReferenceFactory::SatisfySequence(
    const SurfaceSequence& sequence) const {
  if (!manager_)
    return;
  manager_->SatisfySequence(sequence);
}

void DirectSurfaceReferenceFactory::RequireSequence(
    const SurfaceId& surface_id,
    const SurfaceSequence& sequence) const {
  manager_->RequireSequence(surface_id, sequence);
}

}  // namespace cc

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cc/trees/surface_sequence_generator.h>
#include "cc/surfaces/surface_sequence.h"

namespace cc {

SurfaceSequenceGenerator::SurfaceSequenceGenerator()
    : surface_client_id_(0u), next_surface_sequence_(1u) {}

SurfaceSequenceGenerator::~SurfaceSequenceGenerator() = default;

SurfaceSequence SurfaceSequenceGenerator::CreateSurfaceSequence() {
  return SurfaceSequence(surface_client_id_, next_surface_sequence_++);
}

}  // namespace cc

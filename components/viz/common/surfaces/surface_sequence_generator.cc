// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/surface_sequence_generator.h"
#include "components/viz/common/surfaces/surface_sequence.h"

namespace viz {

SurfaceSequenceGenerator::SurfaceSequenceGenerator()
    : next_surface_sequence_(1u) {}

SurfaceSequenceGenerator::~SurfaceSequenceGenerator() = default;

SurfaceSequence SurfaceSequenceGenerator::CreateSurfaceSequence() {
  return SurfaceSequence(frame_sink_id_, next_surface_sequence_++);
}

}  // namespace viz

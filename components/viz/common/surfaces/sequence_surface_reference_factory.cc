// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/sequence_surface_reference_factory.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"

#include "components/viz/common/surfaces/surface_sequence.h"

namespace viz {

base::Closure SequenceSurfaceReferenceFactory::CreateReference(
    SurfaceReferenceOwner* owner,
    const SurfaceId& surface_id) const {
  auto seq = owner->GetSurfaceSequenceGenerator()->CreateSurfaceSequence();
  RequireSequence(surface_id, seq);
  return base::Bind(&SequenceSurfaceReferenceFactory::SatisfySequence, this,
                    seq);
}

}  // namespace viz

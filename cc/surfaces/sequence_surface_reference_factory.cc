// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/sequence_surface_reference_factory.h"

#include "cc/surfaces/sequence_surface_reference.h"

namespace cc {

std::unique_ptr<SurfaceReferenceBase>
SequenceSurfaceReferenceFactory::CreateReference(
    SurfaceReferenceOwner* owner,
    const SurfaceId& surface_id) const {
  auto seq = owner->GetSurfaceSequenceGenerator()->CreateSurfaceSequence();
  RequireSequence(surface_id, seq);
  return base::MakeUnique<SequenceSurfaceReference>(make_scoped_refptr(this),
                                                    seq);
}

void SequenceSurfaceReferenceFactory::DestroyReference(
    SurfaceReferenceBase* surface_ref) const {
  // This method can only be called by a SurfaceReferenceBase because it's
  // private and only SurfaceReferenceBase is a friend. The reference only
  // calls this method on the factory that created it. So it's safe to cast
  // the passed reference to the type returned by CreateReference.
  auto ref = static_cast<SequenceSurfaceReference*>(surface_ref);
  SatisfySequence(ref->sequence());
}

}  // namespace cc

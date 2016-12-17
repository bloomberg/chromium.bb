// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SEQUENCE_SURFACE_REFERENCE_H_
#define CC_SURFACES_SEQUENCE_SURFACE_REFERENCE_H_

#include "cc/surfaces/surface_reference_base.h"
#include "cc/surfaces/surface_reference_factory.h"
#include "cc/surfaces/surface_sequence.h"

namespace cc {

// A surface reference which is internally implemented using SurfaceSequence
class SequenceSurfaceReference final : public SurfaceReferenceBase {
 public:
  SequenceSurfaceReference(scoped_refptr<const SurfaceReferenceFactory> factory,
                           const SurfaceSequence& sequence);

  ~SequenceSurfaceReference() override;

  const SurfaceSequence& sequence() const { return sequence_; }

 private:
  const SurfaceSequence sequence_;

  DISALLOW_COPY_AND_ASSIGN(SequenceSurfaceReference);
};

}  // namespace cc

#endif  // CC_SURFACES_SEQUENCE_SURFACE_REFERENCE_H_

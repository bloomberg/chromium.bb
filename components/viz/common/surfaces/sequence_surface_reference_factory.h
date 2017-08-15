// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_SEQUENCE_SURFACE_REFERENCE_FACTORY_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_SEQUENCE_SURFACE_REFERENCE_FACTORY_H_

#include "components/viz/common/surfaces/surface_reference_factory.h"
#include "components/viz/common/surfaces/surface_sequence.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// A surface reference factory that uses SurfaceSequence.
class VIZ_COMMON_EXPORT SequenceSurfaceReferenceFactory
    : public SurfaceReferenceFactory {
 public:
  SequenceSurfaceReferenceFactory() = default;

  // SurfaceReferenceFactory implementation:
  base::Closure CreateReference(SurfaceReferenceOwner* owner,
                                const SurfaceId& surface_id) const override;

 protected:
  ~SequenceSurfaceReferenceFactory() override = default;

 private:
  virtual void RequireSequence(const SurfaceId& surface_id,
                               const SurfaceSequence& sequence) const = 0;
  virtual void SatisfySequence(const SurfaceSequence& sequence) const = 0;

  DISALLOW_COPY_AND_ASSIGN(SequenceSurfaceReferenceFactory);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_SEQUENCE_SURFACE_REFERENCE_FACTORY_H_

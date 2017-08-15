// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_DIRECT_SURFACE_REFERENCE_FACTORY_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_DIRECT_SURFACE_REFERENCE_FACTORY_H_

#include "base/compiler_specific.h"
#include "components/viz/common/surfaces/sequence_surface_reference_factory.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_sequence.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class SurfaceManager;

// The surface reference factory that interacts directly with SurfaceManager.
// You probably don't need to instantiate this class directly.
// Use SurfaceManager::reference_factory() instead.
class VIZ_SERVICE_EXPORT DirectSurfaceReferenceFactory final
    : public SequenceSurfaceReferenceFactory {
 public:
  explicit DirectSurfaceReferenceFactory(base::WeakPtr<SurfaceManager> manager);

 private:
  ~DirectSurfaceReferenceFactory() override;

  // SequenceSurfaceReferenceFactory implementation:
  void SatisfySequence(const SurfaceSequence& sequence) const override;
  void RequireSequence(const SurfaceId& surface_id,
                       const SurfaceSequence& sequence) const override;

  base::WeakPtr<SurfaceManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(DirectSurfaceReferenceFactory);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_DIRECT_SURFACE_REFERENCE_FACTORY_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_DIRECT_SURFACE_REFERENCE_FACTORY_H_
#define CC_SURFACES_DIRECT_SURFACE_REFERENCE_FACTORY_H_

#include "cc/surfaces/sequence_surface_reference_factory.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_sequence.h"

namespace cc {

class SurfaceManager;

// The surface reference factory that interacts directly with SurfaceManager.
// You probably don't need to instantiate this class directly.
// Use SurfaceManager::reference_factory() instead.
class CC_SURFACES_EXPORT DirectSurfaceReferenceFactory final
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

}  // namespace cc

#endif  // CC_SURFACES_DIRECT_SURFACE_REFERENCE_FACTORY_H_

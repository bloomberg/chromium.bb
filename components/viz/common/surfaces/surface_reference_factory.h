// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_SURFACE_REFERENCE_FACTORY_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_SURFACE_REFERENCE_FACTORY_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_reference_owner.h"

namespace viz {

// Confusingly, SurfaceReferenceFactory is only used to create SurfaceSequences.
// TODO(kylechar): Delete all usage of SurfaceReferenceFactory when surface
// references are enabled by default.
class SurfaceReferenceFactory
    : public base::RefCountedThreadSafe<SurfaceReferenceFactory> {
 public:
  // Creates a reference to the surface with the given surface id and returns
  // a closure that must be called exactly once to remove the reference.
  virtual base::Closure CreateReference(SurfaceReferenceOwner* owner,
                                        const SurfaceId& surface_id) const = 0;

  SurfaceReferenceFactory() = default;

 protected:
  virtual ~SurfaceReferenceFactory() = default;

 private:
  friend class base::RefCountedThreadSafe<SurfaceReferenceFactory>;

  DISALLOW_COPY_AND_ASSIGN(SurfaceReferenceFactory);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_SURFACE_REFERENCE_FACTORY_H_

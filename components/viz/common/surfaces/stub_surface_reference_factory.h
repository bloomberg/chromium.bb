// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_STUB_SURFACE_REFERENCE_FACTORY_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_STUB_SURFACE_REFERENCE_FACTORY_H_

#include "base/compiler_specific.h"
#include "components/viz/common/surfaces/surface_reference_factory.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// A stub implementation that creates a closure which does nothing.
// TODO(kylechar): Delete this class and all usage of
// SurfaceReferenceFactory when surface references are enabled by default.
class VIZ_COMMON_EXPORT StubSurfaceReferenceFactory
    : public NON_EXPORTED_BASE(SurfaceReferenceFactory) {
 public:
  StubSurfaceReferenceFactory() = default;

  // SurfaceReferenceFactory:
  base::Closure CreateReference(SurfaceReferenceOwner* owner,
                                const SurfaceId& surface_id) const override;

 protected:
  ~StubSurfaceReferenceFactory() override = default;

  DISALLOW_COPY_AND_ASSIGN(StubSurfaceReferenceFactory);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_STUB_SURFACE_REFERENCE_FACTORY_H_

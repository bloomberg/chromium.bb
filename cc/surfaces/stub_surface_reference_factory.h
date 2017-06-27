// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_STUB_SURFACE_REFERENCE_FACTORY_H_
#define CC_SURFACES_STUB_SURFACE_REFERENCE_FACTORY_H_

#include "base/compiler_specific.h"
#include "cc/surfaces/surface_reference_factory.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

// A stub implementation that creates a closure which does nothing.
// TODO(kylechar): Delete this class and all usage of SurfaceReferenceFactory
// when surface references are enabled by default.
class CC_SURFACES_EXPORT StubSurfaceReferenceFactory
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

}  // namespace cc

#endif  // CC_SURFACES_STUB_SURFACE_REFERENCE_FACTORY_H_

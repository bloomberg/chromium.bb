// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/stub_surface_reference_factory.h"

#include "base/callback.h"

namespace viz {

base::Closure StubSurfaceReferenceFactory::CreateReference(
    SurfaceReferenceOwner* owner,
    const SurfaceId& surface_id) const {
  return base::Closure();
}

}  // namespace viz

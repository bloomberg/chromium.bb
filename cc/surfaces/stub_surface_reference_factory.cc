// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/stub_surface_reference_factory.h"

#include "base/callback.h"

namespace cc {

base::Closure StubSurfaceReferenceFactory::CreateReference(
    viz::SurfaceReferenceOwner* owner,
    const viz::SurfaceId& surface_id) const {
  return base::Closure();
}

}  // namespace cc

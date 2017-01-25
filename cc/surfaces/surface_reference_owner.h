// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_REFERENCE_OWNER_H_
#define CC_SURFACES_SURFACE_REFERENCE_OWNER_H_

#include "cc/surfaces/surface_sequence_generator.h"

namespace cc {

// Implementations of this interface can be passed to
// SurfaceReferenceFactory::CreateReference as the reference owner.
class SurfaceReferenceOwner {
 public:
  virtual ~SurfaceReferenceOwner() {}

  virtual SurfaceSequenceGenerator* GetSurfaceSequenceGenerator() = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_REFERENCE_OWNER_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_REFERENCE_FACTORY_H_
#define CC_SURFACES_SURFACE_REFERENCE_FACTORY_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_reference_owner.h"

namespace cc {

// Creates surface references. The referenced surface will be kept alive as
// long as there is a reference to it.
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

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_REFERENCE_FACTORY_H_

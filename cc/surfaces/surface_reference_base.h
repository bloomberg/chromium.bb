// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_REFERENCE_BASE_H_
#define CC_SURFACES_SURFACE_REFERENCE_BASE_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "cc/surfaces/surface_sequence.h"

namespace cc {

class SurfaceReferenceFactory;

// The base class for the references returned by SurfaceReferenceFactory and
// its subclasses.
// The objects of this class hold on their surface reference until they go
// out of scope.
// To keep things as lightweight as possible, the base class only keeps a
// pointer to the factory and it's up to the subclasses to decide what other
// information they need to keep.
class SurfaceReferenceBase {
 public:
  explicit SurfaceReferenceBase(
      scoped_refptr<const SurfaceReferenceFactory> factory);

  virtual ~SurfaceReferenceBase();

 protected:
  void Destroy();

 private:
  scoped_refptr<const SurfaceReferenceFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceReferenceBase);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_REFERENCE_BASE_H_

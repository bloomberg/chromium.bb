// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_reference_base.h"

#include "cc/surfaces/surface_reference_factory.h"

namespace cc {

SurfaceReferenceBase::SurfaceReferenceBase(
    scoped_refptr<const SurfaceReferenceFactory> factory)
    : factory_(std::move(factory)) {}

SurfaceReferenceBase::~SurfaceReferenceBase() {
  DCHECK(!factory_) << "Each leaf subclass must call Destroy in its destructor";
}

void SurfaceReferenceBase::Destroy() {
  factory_->DestroyReference(this);
  factory_ = nullptr;
}

}  // namespace cc

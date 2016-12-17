// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/sequence_surface_reference.h"

namespace cc {

SequenceSurfaceReference::SequenceSurfaceReference(
    scoped_refptr<const SurfaceReferenceFactory> factory,
    const SurfaceSequence& sequence)
    : SurfaceReferenceBase(factory), sequence_(sequence) {}

SequenceSurfaceReference::~SequenceSurfaceReference() {
  Destroy();
}

}  // namespace cc

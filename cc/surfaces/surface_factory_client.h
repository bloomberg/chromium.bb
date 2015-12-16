// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_FACTORY_CLIENT_H_
#define CC_SURFACES_SURFACE_FACTORY_CLIENT_H_

#include "cc/resources/returned_resource.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class BeginFrameSource;
struct SurfaceId;

class CC_SURFACES_EXPORT SurfaceFactoryClient {
 public:
  virtual ~SurfaceFactoryClient() {}

  virtual void ReturnResources(const ReturnedResourceArray& resources) = 0;

  virtual void WillDrawSurface(SurfaceId surface_id,
                               const gfx::Rect& damage_rect) {}

  // This allows the SurfaceFactory to tell it's client what BeginFrameSource
  // to use for a given surface_id.
  // If there are multiple active surface_ids, it is the client's
  // responsibility to pick or distribute the correct BeginFrameSource.
  // If surface_id is null, then all BeginFrameSources previously
  // set by this function should be invalidated.
  virtual void SetBeginFrameSource(SurfaceId surface_id,
                                   BeginFrameSource* begin_frame_source) = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_FACTORY_CLIENT_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_H_
#define CC_SURFACES_SURFACE_H_

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"
#include "ui/gfx/size.h"

namespace cc {
class CompositorFrame;
class SurfaceManager;
class SurfaceFactory;
class SurfaceResourceHolder;

class CC_SURFACES_EXPORT Surface {
 public:
  Surface(SurfaceId id, const gfx::Size& size, SurfaceFactory* factory);
  ~Surface();

  const gfx::Size& size() const { return size_; }
  SurfaceId surface_id() const { return surface_id_; }

  void QueueFrame(scoped_ptr<CompositorFrame> frame);
  // Returns the most recent frame that is eligible to be rendered.
  CompositorFrame* GetEligibleFrame();

  SurfaceFactory* factory() { return factory_; }

 private:
  SurfaceId surface_id_;
  gfx::Size size_;
  SurfaceFactory* factory_;
  // TODO(jamesr): Support multiple frames in flight.
  scoped_ptr<CompositorFrame> current_frame_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_H_

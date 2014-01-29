// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_H_
#define CC_SURFACES_SURFACE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/surfaces_export.h"
#include "ui/gfx/size.h"

namespace cc {
class CompositorFrame;
class SurfaceManager;
class SurfaceClient;

class CC_SURFACES_EXPORT Surface {
 public:
  Surface(SurfaceManager* manager,
          SurfaceClient* client,
          const gfx::Size& size);
  ~Surface();

  const gfx::Size& size() const { return size_; }
  int surface_id() const { return surface_id_; }

  void QueueFrame(scoped_ptr<CompositorFrame> frame);
  // Returns the most recent frame that is eligible to be rendered.
  CompositorFrame* GetEligibleFrame();

 private:
  SurfaceManager* manager_;
  SurfaceClient* client_;
  gfx::Size size_;
  int surface_id_;
  // TODO(jamesr): Support multiple frames in flight.
  scoped_ptr<CompositorFrame> current_frame_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_H_

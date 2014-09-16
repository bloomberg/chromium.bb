// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_H_
#define CC_SURFACES_SURFACE_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"
#include "ui/gfx/size.h"

namespace ui {
struct LatencyInfo;
}

namespace cc {
class CompositorFrame;
class CopyOutputRequest;
class SurfaceManager;
class SurfaceFactory;
class SurfaceResourceHolder;

class CC_SURFACES_EXPORT Surface {
 public:
  Surface(SurfaceId id, const gfx::Size& size, SurfaceFactory* factory);
  ~Surface();

  const gfx::Size& size() const { return size_; }
  SurfaceId surface_id() const { return surface_id_; }

  void QueueFrame(scoped_ptr<CompositorFrame> frame,
                  const base::Closure& draw_callback);
  void RequestCopyOfOutput(scoped_ptr<CopyOutputRequest> copy_request);
  void TakeCopyOutputRequests(
      ScopedPtrVector<CopyOutputRequest>* copy_requests);
  // Returns the most recent frame that is eligible to be rendered.
  const CompositorFrame* GetEligibleFrame();

  // Returns a number that increments by 1 every time a new frame is enqueued.
  int frame_index() const { return frame_index_; }

  void TakeLatencyInfo(std::vector<ui::LatencyInfo>* latency_info);
  void RunDrawCallbacks();

  SurfaceFactory* factory() { return factory_; }

 private:
  SurfaceId surface_id_;
  gfx::Size size_;
  SurfaceFactory* factory_;
  // TODO(jamesr): Support multiple frames in flight.
  scoped_ptr<CompositorFrame> current_frame_;
  int frame_index_;
  ScopedPtrVector<CopyOutputRequest> copy_requests_;

  base::Closure draw_callback_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_H_

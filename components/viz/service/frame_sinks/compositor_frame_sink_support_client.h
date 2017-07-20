// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_

#include "cc/resources/returned_resource.h"

namespace cc {
struct BeginFrameArgs;
}  // namespace cc

namespace gfx {
class Rect;
}

namespace viz {
class LocalSurfaceId;

class CompositorFrameSinkSupportClient {
 public:
  // Notification that the previous CompositorFrame given to
  // SubmitCompositorFrame() has been processed and that another frame
  // can be submitted. This provides backpressure from the display compositor
  // so that frames are submitted only at the rate it can handle them.
  // TODO(fsamuel): This method ought not be necessary with unified BeginFrame.
  // However, there's a fair amount of cleanup and refactoring necessary to get
  // rid of it.
  virtual void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) = 0;

  // Notification for the client to generate a CompositorFrame.
  virtual void OnBeginFrame(const cc::BeginFrameArgs& args) = 0;

  // Returns resources sent to SubmitCompositorFrame to be reused or freed.
  virtual void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) = 0;

  // Called when surface is being scheduled for a draw.
  virtual void WillDrawSurface(const LocalSurfaceId& local_surface_id,
                               const gfx::Rect& damage_rect) = 0;

  // Notification that there may not be OnBeginFrame calls for some time.
  virtual void OnBeginFramePausedChanged(bool paused) = 0;

 protected:
  virtual ~CompositorFrameSinkSupportClient() {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_

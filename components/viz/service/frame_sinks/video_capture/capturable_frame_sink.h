// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_CAPTURABLE_FRAME_SINK_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_CAPTURABLE_FRAME_SINK_H_

#include <memory>

#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace viz {

struct BeginFrameAck;
struct BeginFrameArgs;
class CopyOutputRequest;

// Interface for CompositorFrameSink implementations that support frame sink
// video capture.
class CapturableFrameSink {
 public:
  // Interface for a client that observes certain frame events and calls
  // RequestCopyOfSurface() at the appropriate times.
  class Client {
   public:
    virtual ~Client() = default;

    // Called to indicate compositing has started for a new frame.
    virtual void OnBeginFrame(const BeginFrameArgs& args) = 0;

    // Called to indicate a frame's content has changed since the last
    // frame. |ack| identifies the frame. |frame_size| is the output size of the
    // frame, with |damage_rect| being the region within the frame that has
    // changed.
    virtual void OnFrameDamaged(const BeginFrameAck& ack,
                                const gfx::Size& frame_size,
                                const gfx::Rect& damage_rect) = 0;
  };

  virtual ~CapturableFrameSink() = default;

  // Attach/Detach a video capture client to the frame sink. The client will
  // receive frame begin and draw events, and issue copy requests, when
  // appropriate.
  virtual void AttachCaptureClient(Client* client) = 0;
  virtual void DetachCaptureClient(Client* client) = 0;

  // Returns the current surface size.
  virtual gfx::Size GetSurfaceSize() = 0;

  // Issues a request for a copy of the next composited frame.
  virtual void RequestCopyOfSurface(
      std::unique_ptr<CopyOutputRequest> request) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_CAPTURABLE_FRAME_SINK_H_

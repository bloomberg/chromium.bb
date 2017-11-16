// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CONSUMER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CONSUMER_H_

#include <memory>

#include "base/memory/ref_counted.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace media {
class VideoFrame;
}  // namespace media

namespace viz {

class FrameSinkId;
class InFlightFrameDelivery;

// TODO(crbug.com/754872): This is a temporary placeholder; to be replaced with
// a mojo-IDL-generated interface.
class FrameSinkVideoConsumer {
 public:
  virtual ~FrameSinkVideoConsumer() = default;

  // Called to deliver each frame.
  virtual void OnFrameCaptured(
      scoped_refptr<media::VideoFrame> frame,
      const gfx::Rect& update_rect,
      std::unique_ptr<InFlightFrameDelivery> delivery) = 0;

  // Indicates the video capture target (a frame sink) has gone away. A consumer
  // should use this to determine whether to change to a different target or
  // shutdown.
  virtual void OnTargetLost(const FrameSinkId& frame_sink_id) = 0;

  // Indicates that OnFrameCaptured() will not be called again.
  virtual void OnStopped() = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CONSUMER_H_

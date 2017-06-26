// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_COMPOSITOR_FRAME_SINK_DELEGATE_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_COMPOSITOR_FRAME_SINK_DELEGATE_H_

namespace cc {
class FrameSinkId;
}

namespace viz {

class GpuCompositorFrameSinkDelegate {
 public:
  // When the client pipe is closed the host process will be notified.
  virtual void OnClientConnectionLost(const cc::FrameSinkId& frame_sink_id) = 0;

  // When the private pipe is closed the CompositorFrameSink will be destroyed.
  virtual void OnPrivateConnectionLost(
      const cc::FrameSinkId& frame_sink_id) = 0;

 protected:
  virtual ~GpuCompositorFrameSinkDelegate() {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GPU_COMPOSITOR_FRAME_SINK_DELEGATE_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISPLAY_COMPOSITOR_GPU_COMPOSITOR_FRAME_SINK_DELEGATE_H_
#define COMPONENTS_DISPLAY_COMPOSITOR_GPU_COMPOSITOR_FRAME_SINK_DELEGATE_H_

namespace cc {
class FrameSinkId;
}

namespace display_compositor {

class GpuCompositorFrameSinkDelegate {
 public:
  // We must avoid destroying a GpuCompositorFrameSink until both the display
  // compositor host and the client drop their connection to avoid getting into
  // a state where surfaces references are inconsistent.
  virtual void OnClientConnectionLost(const cc::FrameSinkId& frame_sink_id,
                                      bool destroy_compositor_frame_sink) = 0;
  virtual void OnPrivateConnectionLost(const cc::FrameSinkId& frame_sink_id,
                                       bool destroy_compositor_frame_sink) = 0;

 protected:
  virtual ~GpuCompositorFrameSinkDelegate() {}
};

}  // namespace display_compositor

#endif  // COMPONENTS_DISPLAY_COMPOSITOR_GPU_COMPOSITOR_FRAME_SINK_DELEGATE_H_

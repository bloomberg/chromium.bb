// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_TRACE_RECORDER_FOR_VIZ_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_TRACE_RECORDER_FOR_VIZ_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_video_capture.mojom.h"

namespace content {

// Captures snapshots of the page being traced. Used when the
// VizDisplayCompositor feature is enabled.
// TODO(https://crbug.com/813929): Use this class everywhere even if viz is not
// enabled and remove DevToolsFrameTraceRecorder.
class DevToolsFrameTraceRecorderForViz
    : public viz::mojom::FrameSinkVideoConsumer {
 public:
  DevToolsFrameTraceRecorderForViz();
  ~DevToolsFrameTraceRecorderForViz() override;

  void StartCapture();
  void StopCapture();
  void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id);

  // viz::mojom::FrameSinkVideoConsumer implementation.
  void OnFrameCaptured(
      mojo::ScopedSharedBufferHandle buffer,
      uint32_t buffer_size,
      ::media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& update_rect,
      const gfx::Rect& content_rect,
      viz::mojom::FrameSinkVideoConsumerFrameCallbacksPtr callbacks) override;
  void OnTargetLost(const viz::FrameSinkId& frame_sink_id) override;
  void OnStopped() override;

 private:
  viz::mojom::FrameSinkVideoCapturerPtr video_capturer_;
  mojo::Binding<viz::mojom::FrameSinkVideoConsumer> binding_;
  viz::FrameSinkId frame_sink_id_;
  int number_of_screenshots_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DevToolsFrameTraceRecorderForViz);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_TRACE_RECORDER_FOR_VIZ_H_

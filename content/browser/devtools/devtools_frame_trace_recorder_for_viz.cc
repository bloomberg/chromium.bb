// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frame_trace_recorder_for_viz.h"

#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/devtools/devtools_frame_trace_recorder.h"
#include "content/browser/devtools/devtools_traceable_screenshot.h"
#include "media/renderers/paint_canvas_video_renderer.h"

namespace content {

namespace {

static constexpr gfx::Size kMaxFrameSize = gfx::Size(500, 500);

}  // namespace

DevToolsFrameTraceRecorderForViz::DevToolsFrameTraceRecorderForViz()
    : binding_(this) {}

DevToolsFrameTraceRecorderForViz::~DevToolsFrameTraceRecorderForViz() = default;

void DevToolsFrameTraceRecorderForViz::StartCapture() {
  // Early out if we're already capturing,
  if (video_capturer_)
    return;
  GetHostFrameSinkManager()->CreateVideoCapturer(
      mojo::MakeRequest(&video_capturer_));
  video_capturer_->SetResolutionConstraints(gfx::Size(1, 1), kMaxFrameSize,
                                            false);
  video_capturer_->SetMinCapturePeriod(base::TimeDelta::FromMilliseconds(10));
  video_capturer_->SetMinSizeChangePeriod(base::TimeDelta());
  video_capturer_->SetAutoThrottlingEnabled(false);
  viz::mojom::FrameSinkVideoConsumerPtr consumer;
  binding_.Bind(mojo::MakeRequest(&consumer));
  video_capturer_->ChangeTarget(frame_sink_id_);
  video_capturer_->Start(std::move(consumer));
  number_of_screenshots_ = 0;
}

void DevToolsFrameTraceRecorderForViz::StopCapture() {
  if (!video_capturer_)
    return;
  binding_.Close();
  video_capturer_->Stop();
  video_capturer_.reset();
}

void DevToolsFrameTraceRecorderForViz::SetFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
  if (video_capturer_)
    video_capturer_->ChangeTarget(frame_sink_id_);
}

void DevToolsFrameTraceRecorderForViz::OnFrameCaptured(
    mojo::ScopedSharedBufferHandle buffer,
    uint32_t buffer_size,
    ::media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& update_rect,
    const gfx::Rect& content_rect,
    viz::mojom::FrameSinkVideoConsumerFrameCallbacksPtr callbacks) {
  if (!buffer.is_valid()) {
    callbacks->Done();
    return;
  }

  mojo::ScopedSharedBufferMapping mapping = buffer->Map(buffer_size);
  if (!mapping) {
    DLOG(ERROR) << "Shared memory mapping failed.";
    return;
  }

  scoped_refptr<media::VideoFrame> frame;
  frame = media::VideoFrame::WrapExternalData(
      info->pixel_format, info->coded_size, content_rect, content_rect.size(),
      static_cast<uint8_t*>(mapping.get()), buffer_size, info->timestamp);
  if (!frame)
    return;
  frame->AddDestructionObserver(base::BindOnce(
      [](mojo::ScopedSharedBufferMapping mapping) {}, std::move(mapping)));

  media::PaintCanvasVideoRenderer renderer;
  SkBitmap skbitmap;
  skbitmap.allocN32Pixels(content_rect.width(), content_rect.height());
  cc::SkiaPaintCanvas canvas(skbitmap);
  renderer.Copy(frame, &canvas, media::Context3D());
  callbacks->Done();

  DCHECK(info->metadata);
  frame->metadata()->MergeInternalValuesFrom(*info->metadata);
  base::TimeTicks reference_time;
  const bool had_reference_time = frame->metadata()->GetTimeTicks(
      media::VideoFrameMetadata::REFERENCE_TIME, &reference_time);
  DCHECK(had_reference_time);

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID_AND_TIMESTAMP(
      TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"), "Screenshot", 1,
      reference_time,
      std::unique_ptr<base::trace_event::ConvertableToTraceFormat>(
          new DevToolsTraceableScreenshot(skbitmap)));

  ++number_of_screenshots_;
  if (number_of_screenshots_ >=
      DevToolsFrameTraceRecorder::kMaximumNumberOfScreenshots)
    StopCapture();
}

void DevToolsFrameTraceRecorderForViz::OnTargetLost(
    const viz::FrameSinkId& frame_sink_id) {}

void DevToolsFrameTraceRecorderForViz::OnStopped() {}

}  // namespace content

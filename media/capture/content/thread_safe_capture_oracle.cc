// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/content/thread_safe_capture_oracle.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bits.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_util.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

namespace {

// The target maximum amount of the buffer pool to utilize.  Actual buffer pool
// utilization is attenuated by this amount before being reported to the
// VideoCaptureOracle.  This value takes into account the maximum number of
// buffer pool buffers and a desired safety margin.
const int kTargetMaxPoolUtilizationPercent = 60;

}  // namespace

ThreadSafeCaptureOracle::ThreadSafeCaptureOracle(
    std::unique_ptr<VideoCaptureDevice::Client> client,
    const VideoCaptureParams& params,
    bool enable_auto_throttling)
    : client_(std::move(client)),
      oracle_(base::TimeDelta::FromMicroseconds(static_cast<int64_t>(
                  1000000.0 / params.requested_format.frame_rate +
                  0.5 /* to round to nearest int */)),
              params.requested_format.frame_size,
              params.resolution_change_policy,
              enable_auto_throttling),
      params_(params) {}

ThreadSafeCaptureOracle::~ThreadSafeCaptureOracle() {
}

bool ThreadSafeCaptureOracle::ObserveEventAndDecideCapture(
    VideoCaptureOracle::Event event,
    const gfx::Rect& damage_rect,
    base::TimeTicks event_time,
    scoped_refptr<VideoFrame>* storage,
    CaptureFrameCallback* callback) {
  // Grab the current time before waiting to acquire the |lock_|.
  const base::TimeTicks capture_begin_time = base::TimeTicks::Now();

  gfx::Size visible_size;
  gfx::Size coded_size;
  media::VideoCaptureDevice::Client::Buffer output_buffer;
  double attenuated_utilization;
  int frame_number;
  base::TimeDelta estimated_frame_duration;
  {
    base::AutoLock guard(lock_);

    if (!client_)
      return false;  // Capture is stopped.

    if (!oracle_.ObserveEventAndDecideCapture(event, damage_rect, event_time)) {
      // This is a normal and acceptable way to drop a frame. We've hit our
      // capture rate limit: for example, the content is animating at 60fps but
      // we're capturing at 30fps.
      TRACE_EVENT_INSTANT1("gpu.capture", "FpsRateLimited",
                           TRACE_EVENT_SCOPE_THREAD, "trigger",
                           VideoCaptureOracle::EventAsString(event));
      return false;
    }

    frame_number = oracle_.next_frame_number();
    visible_size = oracle_.capture_size();
    // TODO(miu): Clients should request exact padding, instead of this
    // memory-wasting hack to make frames that are compatible with all HW
    // encoders.  http://crbug.com/555911
    coded_size.SetSize(base::bits::Align(visible_size.width(), 16),
                       base::bits::Align(visible_size.height(), 16));

    if (event == VideoCaptureOracle::kPassiveRefreshRequest) {
      output_buffer = client_->ResurrectLastOutputBuffer(
          coded_size, params_.requested_format.pixel_format,
          params_.requested_format.pixel_storage, frame_number);
      if (!output_buffer.is_valid()) {
        TRACE_EVENT_INSTANT0("gpu.capture", "ResurrectionFailed",
                             TRACE_EVENT_SCOPE_THREAD);
        return false;
      }
    } else {
      output_buffer = client_->ReserveOutputBuffer(
          coded_size, params_.requested_format.pixel_format,
          params_.requested_format.pixel_storage, frame_number);
    }

    // Get the current buffer pool utilization and attenuate it: The utilization
    // reported to the oracle is in terms of a maximum sustainable amount (not
    // the absolute maximum).
    attenuated_utilization = client_->GetBufferPoolUtilization() *
                             (100.0 / kTargetMaxPoolUtilizationPercent);

    if (!output_buffer.is_valid()) {
      TRACE_EVENT_INSTANT2(
          "gpu.capture", "PipelineLimited", TRACE_EVENT_SCOPE_THREAD, "trigger",
          VideoCaptureOracle::EventAsString(event), "atten_util_percent",
          base::saturated_cast<int>(attenuated_utilization * 100.0 + 0.5));
      oracle_.RecordWillNotCapture(attenuated_utilization);
      return false;
    }

    oracle_.RecordCapture(attenuated_utilization);
    estimated_frame_duration = oracle_.estimated_frame_duration();
  }  // End of critical section.

  if (attenuated_utilization >= 1.0) {
    TRACE_EVENT_INSTANT2(
        "gpu.capture", "NearlyPipelineLimited", TRACE_EVENT_SCOPE_THREAD,
        "trigger", VideoCaptureOracle::EventAsString(event),
        "atten_util_percent",
        base::saturated_cast<int>(attenuated_utilization * 100.0 + 0.5));
  }

  TRACE_EVENT_ASYNC_BEGIN2("gpu.capture", "Capture", output_buffer.id,
                           "frame_number", frame_number, "trigger",
                           VideoCaptureOracle::EventAsString(event));

  auto output_buffer_access =
      output_buffer.handle_provider->GetHandleForInProcessAccess();
  DCHECK_EQ(media::PIXEL_STORAGE_CPU, params_.requested_format.pixel_storage);
  *storage = VideoFrame::WrapExternalSharedMemory(
      params_.requested_format.pixel_format, coded_size,
      gfx::Rect(visible_size), visible_size, output_buffer_access->data(),
      output_buffer_access->mapped_size(), base::SharedMemory::NULLHandle(), 0u,
      base::TimeDelta());
  // If creating the VideoFrame wrapper failed, call DidCaptureFrame() with
  // !success to execute the required post-capture steps (tracing, notification
  // of failure to VideoCaptureOracle, etc.).
  if (!(*storage)) {
    DidCaptureFrame(frame_number, std::move(output_buffer), capture_begin_time,
                    estimated_frame_duration, *storage, event_time, false);
    return false;
  }

  *callback = base::Bind(&ThreadSafeCaptureOracle::DidCaptureFrame, this,
                         frame_number, base::Passed(&output_buffer),
                         capture_begin_time, estimated_frame_duration);

  return true;
}

bool ThreadSafeCaptureOracle::AttemptPassiveRefresh() {
  const base::TimeTicks refresh_time = base::TimeTicks::Now();

  scoped_refptr<VideoFrame> frame;
  CaptureFrameCallback capture_callback;
  if (!ObserveEventAndDecideCapture(VideoCaptureOracle::kPassiveRefreshRequest,
                                    gfx::Rect(), refresh_time, &frame,
                                    &capture_callback)) {
    return false;
  }

  capture_callback.Run(std::move(frame), refresh_time, true);
  return true;
}

gfx::Size ThreadSafeCaptureOracle::GetCaptureSize() const {
  base::AutoLock guard(lock_);
  return oracle_.capture_size();
}

void ThreadSafeCaptureOracle::UpdateCaptureSize(const gfx::Size& source_size) {
  base::AutoLock guard(lock_);
  VLOG(1) << "Source size changed to " << source_size.ToString();
  oracle_.SetSourceSize(source_size);
}

void ThreadSafeCaptureOracle::Stop() {
  base::AutoLock guard(lock_);
  client_.reset();
}

void ThreadSafeCaptureOracle::ReportError(
    const tracked_objects::Location& from_here,
    const std::string& reason) {
  base::AutoLock guard(lock_);
  if (client_)
    client_->OnError(from_here, reason);
}

void ThreadSafeCaptureOracle::ReportStarted() {
  base::AutoLock guard(lock_);
  if (client_)
    client_->OnStarted();
}

void ThreadSafeCaptureOracle::DidCaptureFrame(
    int frame_number,
    VideoCaptureDevice::Client::Buffer buffer,
    base::TimeTicks capture_begin_time,
    base::TimeDelta estimated_frame_duration,
    scoped_refptr<VideoFrame> frame,
    base::TimeTicks reference_time,
    bool success) {
  TRACE_EVENT_ASYNC_END2("gpu.capture", "Capture", buffer.id, "success",
                         success, "timestamp",
                         reference_time.ToInternalValue());

  base::AutoLock guard(lock_);

  if (!oracle_.CompleteCapture(frame_number, success, &reference_time))
    return;

  TRACE_EVENT_INSTANT0("gpu.capture", "CaptureSucceeded",
                       TRACE_EVENT_SCOPE_THREAD);

  if (!client_)
    return;  // Capture is stopped.

  frame->metadata()->SetDouble(VideoFrameMetadata::FRAME_RATE,
                               params_.requested_format.frame_rate);
  frame->metadata()->SetTimeTicks(VideoFrameMetadata::CAPTURE_BEGIN_TIME,
                                  capture_begin_time);
  frame->metadata()->SetTimeTicks(VideoFrameMetadata::CAPTURE_END_TIME,
                                  base::TimeTicks::Now());
  frame->metadata()->SetTimeDelta(VideoFrameMetadata::FRAME_DURATION,
                                  estimated_frame_duration);
  frame->metadata()->SetTimeTicks(VideoFrameMetadata::REFERENCE_TIME,
                                  reference_time);

  DCHECK(frame->IsMappable());
  media::VideoCaptureFormat format(frame->coded_size(),
                                   params_.requested_format.frame_rate,
                                   frame->format(), media::PIXEL_STORAGE_CPU);
  client_->OnIncomingCapturedBufferExt(
      std::move(buffer), format, reference_time, frame->timestamp(),
      frame->visible_rect(), *frame->metadata());
}

void ThreadSafeCaptureOracle::OnConsumerReportingUtilization(
    int frame_number,
    double utilization) {
  base::AutoLock guard(lock_);
  oracle_.RecordConsumerFeedback(frame_number, utilization);
}

}  // namespace media

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_impl.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_manager.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_consumer.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using media::VideoCaptureOracle;
using media::VideoFrame;
using media::VideoFrameMetadata;

namespace viz {

// static
constexpr media::VideoPixelFormat
    FrameSinkVideoCapturerImpl::kDefaultPixelFormat;

// static
constexpr media::ColorSpace FrameSinkVideoCapturerImpl::kDefaultColorSpace;

FrameSinkVideoCapturerImpl::FrameSinkVideoCapturerImpl(
    FrameSinkVideoCapturerManager* frame_sink_manager)
    : frame_sink_manager_(frame_sink_manager),
      copy_request_source_(base::UnguessableToken::Create()),
      oracle_(true /* enable_auto_throttling */),
      frame_pool_(kDesignLimitMaxFrames),
      feedback_weak_factory_(&oracle_),
      capture_weak_factory_(this) {
  DCHECK(frame_sink_manager_);
}

FrameSinkVideoCapturerImpl::~FrameSinkVideoCapturerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Stop();
  SetResolvedTarget(nullptr);
}

void FrameSinkVideoCapturerImpl::SetResolvedTarget(
    CapturableFrameSink* target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (resolved_target_ == target) {
    return;
  }

  if (resolved_target_) {
    resolved_target_->DetachCaptureClient(this);
  }
  resolved_target_ = target;
  if (resolved_target_) {
    resolved_target_->AttachCaptureClient(this);
    const gfx::Size& source_size = resolved_target_->GetSurfaceSize();
    if (source_size != oracle_.source_size()) {
      oracle_.SetSourceSize(source_size);
    }
    MaybeCaptureFrame(VideoCaptureOracle::kActiveRefreshRequest,
                      gfx::Rect(source_size), clock_->NowTicks());
  } else {
    // Not calling consumer_->OnTargetLost() because SetResolvedTarget() should
    // be called by FrameSinkManagerImpl with a valid target very soon.
  }
}

void FrameSinkVideoCapturerImpl::OnTargetWillGoAway() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!resolved_target_) {
    return;
  }
  resolved_target_->DetachCaptureClient(this);
  resolved_target_ = nullptr;

  if (requested_target_.is_valid()) {
    if (consumer_) {
      consumer_->OnTargetLost(requested_target_);
    }
    requested_target_ = FrameSinkId();
  }
}

void FrameSinkVideoCapturerImpl::SetFormat(media::VideoPixelFormat format,
                                           media::ColorSpace color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (format != media::PIXEL_FORMAT_I420 &&
      format != media::PIXEL_FORMAT_YV12) {
    LOG(DFATAL) << "Invalid pixel format: Only I420 or YV12 are supported.";
  } else {
    pixel_format_ = format;
  }

  if (color_space == media::COLOR_SPACE_UNSPECIFIED) {
    color_space = kDefaultColorSpace;
  }
  // TODO(crbug/758057): Plumb output color space through to the
  // CopyOutputRequests.
  if (color_space != media::COLOR_SPACE_HD_REC709) {
    LOG(DFATAL) << "Unsupported color space: Only BT.709 is supported.";
  } else {
    color_space_ = color_space;
  }
}

void FrameSinkVideoCapturerImpl::SetMinCapturePeriod(
    base::TimeDelta min_capture_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  constexpr base::TimeDelta kMinMinCapturePeriod =
      base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                        media::limits::kMaxFramesPerSecond);
  if (min_capture_period < kMinMinCapturePeriod) {
    min_capture_period = kMinMinCapturePeriod;
  }

  // On machines without high-resolution clocks, limit the maximum frame rate to
  // 30 FPS. This avoids a potential issue where the system clock may not
  // advance between two successive frames.
  if (!base::TimeTicks::IsHighResolution()) {
    constexpr base::TimeDelta kMinLowResCapturePeriod =
        base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                          30);
    if (min_capture_period < kMinLowResCapturePeriod) {
      min_capture_period = kMinLowResCapturePeriod;
    }
  }

  oracle_.SetMinCapturePeriod(min_capture_period);
}

void FrameSinkVideoCapturerImpl::SetResolutionConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size,
    bool use_fixed_aspect_ratio) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (min_size.width() <= 0 || min_size.height() <= 0 ||
      max_size.width() > media::limits::kMaxDimension ||
      max_size.height() > media::limits::kMaxDimension ||
      min_size.width() > max_size.width() ||
      min_size.height() > max_size.height()) {
    LOG(DFATAL) << "Invalid resolutions constraints: " << min_size.ToString()
                << " must not be greater than " << max_size.ToString()
                << "; and also within media::limits.";
    return;
  }

  oracle_.SetCaptureSizeConstraints(min_size, max_size, use_fixed_aspect_ratio);
}

void FrameSinkVideoCapturerImpl::ChangeTarget(
    const FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  requested_target_ = frame_sink_id;
  SetResolvedTarget(
      frame_sink_manager_->FindCapturableFrameSink(frame_sink_id));
}

void FrameSinkVideoCapturerImpl::Start(
    std::unique_ptr<FrameSinkVideoConsumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Stop();
  consumer_ = std::move(consumer);
  MaybeCaptureFrame(VideoCaptureOracle::kActiveRefreshRequest,
                    gfx::Rect(oracle_.source_size()), clock_->NowTicks());
}

void FrameSinkVideoCapturerImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel any captures in-flight and any captured frames pending delivery.
  capture_weak_factory_.InvalidateWeakPtrs();
  oracle_.CancelAllCaptures();
  while (!delivery_queue_.empty()) {
    delivery_queue_.pop();
  }
  next_delivery_frame_number_ = next_capture_frame_number_;

  if (consumer_) {
    consumer_->OnStopped();
    consumer_.reset();
  }
}

void FrameSinkVideoCapturerImpl::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MaybeCaptureFrame(VideoCaptureOracle::kPassiveRefreshRequest,
                    gfx::Rect(oracle_.source_size()), clock_->NowTicks());
}

void FrameSinkVideoCapturerImpl::OnBeginFrame(const BeginFrameArgs& args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(args.IsValid());
  DCHECK(resolved_target_);

  frame_display_times_[args.sequence_number % frame_display_times_.size()] =
      args.frame_time + args.interval;
  current_begin_frame_source_id_ = args.source_id;
}

void FrameSinkVideoCapturerImpl::OnFrameDamaged(const BeginFrameAck& ack,
                                                const gfx::Size& frame_size,
                                                const gfx::Rect& damage_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!frame_size.IsEmpty());
  DCHECK(!damage_rect.IsEmpty());
  DCHECK(resolved_target_);

  if (ack.source_id != current_begin_frame_source_id_ ||
      ack.sequence_number < BeginFrameArgs::kStartingFrameNumber) {
    return;
  }

  if (frame_size != oracle_.source_size()) {
    oracle_.SetSourceSize(frame_size);
  }

  MaybeCaptureFrame(
      VideoCaptureOracle::kCompositorUpdate, damage_rect,
      frame_display_times_[ack.sequence_number % frame_display_times_.size()]);
}

void FrameSinkVideoCapturerImpl::MaybeCaptureFrame(
    VideoCaptureOracle::Event event,
    const gfx::Rect& damage_rect,
    base::TimeTicks event_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Consult the oracle to determine whether this frame should be captured.
  if (!oracle_.ObserveEventAndDecideCapture(event, damage_rect, event_time)) {
    TRACE_EVENT_INSTANT1("gpu.capture", "FpsRateLimited",
                         TRACE_EVENT_SCOPE_THREAD, "trigger",
                         VideoCaptureOracle::EventAsString(event));
    return;
  }

  // If there is no |consumer_| present, punt. This check is being done after
  // consulting the oracle because it helps to "prime" the oracle in the short
  // period of time where the capture target is known but the |consumer_| has
  // not yet been provided in the call to Start().
  if (!consumer_) {
    TRACE_EVENT_INSTANT1("gpu.capture", "NoConsumer", TRACE_EVENT_SCOPE_THREAD,
                         "trigger", VideoCaptureOracle::EventAsString(event));
    return;
  }

  // Reserve a buffer from the pool for the next frame.
  const OracleFrameNumber oracle_frame_number = oracle_.next_frame_number();
  scoped_refptr<VideoFrame> frame;
  if (event == VideoCaptureOracle::kPassiveRefreshRequest) {
    frame = frame_pool_.ResurrectLastVideoFrame(oracle_.capture_size(),
                                                pixel_format_);
    // If the resurrection failed, promote the passive refresh request to an
    // active refresh request and retry.
    if (!frame) {
      TRACE_EVENT_INSTANT0("gpu.capture", "ResurrectionFailed",
                           TRACE_EVENT_SCOPE_THREAD);
      MaybeCaptureFrame(VideoCaptureOracle::kActiveRefreshRequest, damage_rect,
                        event_time);
      return;
    }
  } else {
    frame =
        frame_pool_.ReserveVideoFrame(oracle_.capture_size(), pixel_format_);
  }

  // Compute the current in-flight utilization and attenuate it: The utilization
  // reported to the oracle is in terms of a maximum sustainable amount (not the
  // absolute maximum).
  const float utilization =
      frame_pool_.GetUtilization() / kTargetPipelineUtilization;

  // Do not proceed if the pool did not provide a frame: This indicates the
  // pipeline is full.
  if (!frame) {
    TRACE_EVENT_INSTANT2(
        "gpu.capture", "PipelineLimited", TRACE_EVENT_SCOPE_THREAD, "trigger",
        VideoCaptureOracle::EventAsString(event), "atten_util_percent",
        base::saturated_cast<int>(utilization * 100.0f + 0.5f));
    oracle_.RecordWillNotCapture(utilization);
    return;
  }

  // Record a trace event if the capture pipeline is redlining, but capture will
  // still proceed.
  if (utilization >= 1.0) {
    TRACE_EVENT_INSTANT2(
        "gpu.capture", "NearlyPipelineLimited", TRACE_EVENT_SCOPE_THREAD,
        "trigger", VideoCaptureOracle::EventAsString(event),
        "atten_util_percent",
        base::saturated_cast<int>(utilization * 100.0f + 0.5f));
  }

  // At this point, the capture is going to proceed. Populate the VideoFrame's
  // metadata, and notify the oracle.
  VideoFrameMetadata* const metadata = frame->metadata();
  metadata->SetTimeTicks(VideoFrameMetadata::CAPTURE_BEGIN_TIME,
                         clock_->NowTicks());
  // See TODO in SetFormat(). For now, always assume Rec. 709.
  frame->set_color_space(gfx::ColorSpace::CreateREC709());
  metadata->SetInteger(VideoFrameMetadata::COLOR_SPACE,
                       media::COLOR_SPACE_HD_REC709);
  metadata->SetTimeDelta(VideoFrameMetadata::FRAME_DURATION,
                         oracle_.estimated_frame_duration());
  metadata->SetDouble(VideoFrameMetadata::FRAME_RATE,
                      1.0 / oracle_.min_capture_period().InSecondsF());
  metadata->SetTimeTicks(VideoFrameMetadata::REFERENCE_TIME, event_time);

  oracle_.RecordCapture(utilization);
  const int64_t frame_number = next_capture_frame_number_++;
  TRACE_EVENT_ASYNC_BEGIN2("gpu.capture", "Capture", frame.get(),
                           "frame_number", frame_number, "trigger",
                           VideoCaptureOracle::EventAsString(event));

  // If there is currently no resolved target (or the target has zero area),
  // deliver a blank black frame.
  const gfx::Size& source_size = oracle_.source_size();
  if (!resolved_target_ || source_size.IsEmpty()) {
    media::FillYUV(frame.get(), 0x00, 0x80, 0x80);
    DidCaptureFrame(frame_number, oracle_frame_number, std::move(frame));
    return;
  }

  // For passive refresh requests, just deliver the resurrected frame.
  if (event == VideoCaptureOracle::kPassiveRefreshRequest) {
    DidCaptureFrame(frame_number, oracle_frame_number, std::move(frame));
    return;
  }

  // Request a copy of the next frame from the frame sink.
  const gfx::Rect content_rect =
      media::ComputeLetterboxRegionForI420(frame->visible_rect(), source_size);
  std::unique_ptr<CopyOutputRequest> request(new CopyOutputRequest(
      CopyOutputRequest::ResultFormat::I420_PLANES,
      base::BindOnce(&FrameSinkVideoCapturerImpl::DidCopyFrame,
                     capture_weak_factory_.GetWeakPtr(), frame_number,
                     oracle_frame_number, std::move(frame), content_rect)));
  request->set_source(copy_request_source_);
  request->set_area(gfx::Rect(source_size));
  request->SetScaleRatio(
      gfx::Vector2d(source_size.width(), source_size.height()),
      gfx::Vector2d(content_rect.width(), content_rect.height()));
  // TODO(crbug.com/775740): As an optimization, set the result selection to
  // just the part of the result that would have changed due to aggregated
  // damage over all the frames that weren't captured.
  request->set_result_selection(gfx::Rect(content_rect.size()));
  resolved_target_->RequestCopyOfSurface(std::move(request));
}

void FrameSinkVideoCapturerImpl::DidCopyFrame(
    int64_t frame_number,
    OracleFrameNumber oracle_frame_number,
    scoped_refptr<VideoFrame> frame,
    const gfx::Rect& content_rect,
    std::unique_ptr<CopyOutputResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(frame_number, next_delivery_frame_number_);
  DCHECK(frame);
  DCHECK_EQ(content_rect.x() % 2, 0);
  DCHECK_EQ(content_rect.y() % 2, 0);
  DCHECK_EQ(content_rect.width() % 2, 0);
  DCHECK_EQ(content_rect.height() % 2, 0);
  DCHECK(result);

  // Stop() should have canceled any outstanding copy requests. So, by reaching
  // this point, there should be a |consumer_| present.
  DCHECK(consumer_);

  // Populate the VideoFrame from the CopyOutputResult.
  const int y_stride = frame->stride(VideoFrame::kYPlane);
  uint8_t* const y = frame->visible_data(VideoFrame::kYPlane) +
                     content_rect.y() * y_stride + content_rect.x();
  const int u_stride = frame->stride(VideoFrame::kUPlane);
  uint8_t* const u = frame->visible_data(VideoFrame::kUPlane) +
                     (content_rect.y() / 2) * u_stride + (content_rect.x() / 2);
  const int v_stride = frame->stride(VideoFrame::kVPlane);
  uint8_t* const v = frame->visible_data(VideoFrame::kVPlane) +
                     (content_rect.y() / 2) * v_stride + (content_rect.x() / 2);
  if (result->ReadI420Planes(y, y_stride, u, u_stride, v, v_stride)) {
    media::LetterboxYUV(frame.get(), content_rect);
  } else {
    frame = nullptr;
  }

  DidCaptureFrame(frame_number, oracle_frame_number, std::move(frame));
}

void FrameSinkVideoCapturerImpl::DidCaptureFrame(
    int64_t frame_number,
    OracleFrameNumber oracle_frame_number,
    scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(frame_number, next_delivery_frame_number_);

  if (frame) {
    frame->metadata()->SetTimeTicks(VideoFrameMetadata::CAPTURE_END_TIME,
                                    clock_->NowTicks());
  }

  // Ensure frames are delivered in-order by using a min-heap, and only
  // deliver the next frame(s) in-sequence when they are found at the top.
  delivery_queue_.emplace(frame_number, oracle_frame_number, std::move(frame));
  while (delivery_queue_.top().frame_number == next_delivery_frame_number_) {
    MaybeDeliverFrame(delivery_queue_.top().oracle_frame_number,
                      std::move(delivery_queue_.top().frame));
    ++next_delivery_frame_number_;
    delivery_queue_.pop();
    if (delivery_queue_.empty()) {
      break;
    }
  }
}

void FrameSinkVideoCapturerImpl::MaybeDeliverFrame(
    OracleFrameNumber oracle_frame_number,
    scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The Oracle has the final say in whether frame delivery will proceed. It
  // also rewrites the media timestamp in terms of the smooth flow of the
  // original source content.
  base::TimeTicks media_ticks;
  const bool should_deliver_frame =
      oracle_.CompleteCapture(oracle_frame_number, !!frame, &media_ticks);
  DCHECK(frame || !should_deliver_frame);

  // The following is used by
  // chrome/browser/extension/api/cast_streaming/performance_test.cc, in
  // addition to the usual runtime tracing.
  TRACE_EVENT_ASYNC_END2("gpu.capture", "Capture", frame.get(), "success",
                         should_deliver_frame, "timestamp",
                         (media_ticks - base::TimeTicks()).InMicroseconds());

  if (!should_deliver_frame) {
    return;
  }

  // Set media timestamp in terms of the time offset since the first frame.
  if (!first_frame_media_ticks_) {
    first_frame_media_ticks_ = media_ticks;
  }
  frame->set_timestamp(media_ticks - *first_frame_media_ticks_);

  // Construct a new InFlightFrameDelivery instance, to provide the done signal
  // for this frame and deliver the frame to the consumer.
  const gfx::Rect update_rect = frame->visible_rect();
  auto delivery = std::make_unique<InFlightFrameDelivery>(
      frame_pool_.HoldFrameForDelivery(frame.get()),
      base::BindOnce(&VideoCaptureOracle::RecordConsumerFeedback,
                     feedback_weak_factory_.GetWeakPtr(), oracle_frame_number));
  consumer_->OnFrameCaptured(std::move(frame), update_rect,
                             std::move(delivery));
}

FrameSinkVideoCapturerImpl::CapturedFrame::CapturedFrame(
    int64_t fn,
    OracleFrameNumber ofn,
    scoped_refptr<VideoFrame> fr)
    : frame_number(fn), oracle_frame_number(ofn), frame(std::move(fr)) {}

FrameSinkVideoCapturerImpl::CapturedFrame::CapturedFrame(
    const CapturedFrame& other) = default;

FrameSinkVideoCapturerImpl::CapturedFrame::~CapturedFrame() = default;

bool FrameSinkVideoCapturerImpl::CapturedFrame::operator<(
    const FrameSinkVideoCapturerImpl::CapturedFrame& other) const {
  // Reverse the sort order; so std::priority_queue<CapturedFrame> becomes a
  // min-heap instead of a max-heap.
  return other.frame_number < frame_number;
}

}  // namespace viz

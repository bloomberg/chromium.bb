// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_manager.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "media/capture/mojo/video_capture_types.mojom.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

using media::VideoCaptureOracle;
using media::VideoFrame;
using media::VideoFrameMetadata;

namespace viz {

namespace {
// Returns |raw_size| truncated to positive even-numbered values.
gfx::Size AdjustSizeForI420(const gfx::Size& raw_size) {
  gfx::Size result(raw_size.width() & ~1, raw_size.height() & ~1);
  if (result.width() <= 0) {
    result.set_width(2);
  }
  if (result.height() <= 0) {
    result.set_height(2);
  }
  return result;
}
}  // namespace

// static
constexpr media::VideoPixelFormat
    FrameSinkVideoCapturerImpl::kDefaultPixelFormat;

// static
constexpr media::ColorSpace FrameSinkVideoCapturerImpl::kDefaultColorSpace;

// static
constexpr base::TimeDelta
    FrameSinkVideoCapturerImpl::kRefreshFrameRetryInterval;

// static
constexpr base::TimeDelta
    FrameSinkVideoCapturerImpl::kDisplayTimeCacheKeepAliveInterval;

FrameSinkVideoCapturerImpl::FrameSinkVideoCapturerImpl(
    FrameSinkVideoCapturerManager* frame_sink_manager,
    mojom::FrameSinkVideoCapturerRequest request)
    : frame_sink_manager_(frame_sink_manager),
      binding_(this),
      copy_request_source_(base::UnguessableToken::Create()),
      clock_(base::DefaultTickClock::GetInstance()),
      oracle_(true /* enable_auto_throttling */),
      frame_pool_(kDesignLimitMaxFrames),
      feedback_weak_factory_(&oracle_),
      capture_weak_factory_(this) {
  DCHECK(frame_sink_manager_);

  if (request.is_pending()) {
    binding_.Bind(std::move(request));
    binding_.set_connection_error_handler(
        base::BindOnce(&FrameSinkVideoCapturerManager::OnCapturerConnectionLost,
                       base::Unretained(frame_sink_manager_), this));
  }
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

  if (format != media::PIXEL_FORMAT_I420) {
    LOG(DFATAL) << "Invalid pixel format: Only I420 supported.";
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
    mojom::FrameSinkVideoConsumerPtr consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(consumer);

  Stop();
  consumer_ = std::move(consumer);
  // In the future, if the connection to the consumer is lost before a call to
  // Stop(), make that call on its behalf.
  consumer_.set_connection_error_handler(base::BindOnce(
      &FrameSinkVideoCapturerImpl::Stop, base::Unretained(this)));
  MaybeCaptureFrame(VideoCaptureOracle::kActiveRefreshRequest,
                    gfx::Rect(oracle_.source_size()), clock_->NowTicks());
}

void FrameSinkVideoCapturerImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  refresh_frame_retry_timer_.Stop();

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

  // Note: It's possible that there are multiple BeginFrameSources that may call
  // this method. It's not possible to know which one will be associated with a
  // later OnFrameDamaged() call, so all recent timestamps must be cached.

  const size_t prior_source_count = frame_display_times_.size();
  TimeRingBuffer& ring_buffer = frame_display_times_[args.source_id];
  const base::TimeTicks display_time = args.frame_time + args.interval;
  DCHECK(!display_time.is_null());
  ring_buffer[args.sequence_number % ring_buffer.size()] = display_time;

  // Garbage-collect |frame_display_times_| entries that are no longer being
  // actively updated. This only runs when this method is being called with an
  // as-yet-unseen |args.source_id|. An entry is pruned only if all of its
  // timestamps are older than a reasonable threshold.
  if (frame_display_times_.size() != prior_source_count) {
    const base::TimeTicks threshold =
        display_time - kDisplayTimeCacheKeepAliveInterval;
    using KeyValuePair = decltype(frame_display_times_)::value_type;
    base::EraseIf(frame_display_times_, [&threshold](const KeyValuePair& p) {
      const TimeRingBuffer& ring_buffer = p.second;
      return std::all_of(ring_buffer.begin(), ring_buffer.end(),
                         [&threshold](base::TimeTicks t) {
                           return t.is_null() || t < threshold;
                         });
    });
  }
}

void FrameSinkVideoCapturerImpl::OnFrameDamaged(const BeginFrameAck& ack,
                                                const gfx::Size& frame_size,
                                                const gfx::Rect& damage_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!frame_size.IsEmpty());
  DCHECK(!damage_rect.IsEmpty());
  DCHECK(resolved_target_);

  base::TimeTicks display_time;
  const auto it = frame_display_times_.find(ack.source_id);
  if (it != frame_display_times_.end()) {
    const TimeRingBuffer& ring_buffer = it->second;
    display_time = ring_buffer[ack.sequence_number % ring_buffer.size()];
  }
  if (display_time.is_null()) {
    // This can sometimes occur for the first few frames when capture starts,
    // but should not happen thereafter.
    DLOG(WARNING) << "OnFrameDamaged() called without prior OnBeginFrame().";
    return;
  }

  if (frame_size != oracle_.source_size()) {
    oracle_.SetSourceSize(frame_size);
  }

  MaybeCaptureFrame(VideoCaptureOracle::kCompositorUpdate, damage_rect,
                    display_time);
}

void FrameSinkVideoCapturerImpl::MaybeCaptureFrame(
    VideoCaptureOracle::Event event,
    const gfx::Rect& damage_rect,
    base::TimeTicks event_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Consult the oracle to determine whether this frame should be captured.
  if (oracle_.ObserveEventAndDecideCapture(event, damage_rect, event_time)) {
    // Regardless of the type of |event|, there is no longer a need for the
    // refresh frame retry timer to fire. The following is a no-op, if the timer
    // was not running.
    refresh_frame_retry_timer_.Stop();
  } else {
    TRACE_EVENT_INSTANT1("gpu.capture", "FpsRateLimited",
                         TRACE_EVENT_SCOPE_THREAD, "trigger",
                         VideoCaptureOracle::EventAsString(event));

    // If the oracle rejected a "refresh frame" request, schedule a later retry.
    if (event == VideoCaptureOracle::kPassiveRefreshRequest ||
        event == VideoCaptureOracle::kActiveRefreshRequest) {
      refresh_frame_retry_timer_.Start(
          FROM_HERE,
          std::max(kRefreshFrameRetryInterval, oracle_.min_capture_period()),
          base::BindRepeating(
              [](FrameSinkVideoCapturerImpl* self,
                 VideoCaptureOracle::Event event) {
                self->MaybeCaptureFrame(event,
                                        gfx::Rect(self->oracle_.source_size()),
                                        self->clock_->NowTicks());
              },
              this, event));
    }
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
  const gfx::Size i420_capture_size = AdjustSizeForI420(oracle_.capture_size());
  scoped_refptr<VideoFrame> frame;
  if (event == VideoCaptureOracle::kPassiveRefreshRequest) {
    frame =
        frame_pool_.ResurrectLastVideoFrame(i420_capture_size, pixel_format_);
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
    frame = frame_pool_.ReserveVideoFrame(i420_capture_size, pixel_format_);
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
    DidCaptureFrame(frame_number, oracle_frame_number, std::move(frame),
                    gfx::Rect());
    return;
  }

  const gfx::Rect content_rect =
      media::ComputeLetterboxRegionForI420(frame->visible_rect(), source_size);

  // For passive refresh requests, just deliver the resurrected frame.
  if (event == VideoCaptureOracle::kPassiveRefreshRequest) {
    DidCaptureFrame(frame_number, oracle_frame_number, std::move(frame),
                    content_rect);
    return;
  }

  // Request a copy of the next frame from the frame sink.
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
  // this point, |consumer_| should be bound.
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
    // The result may be smaller than what was requested, if unforeseen clamping
    // to the source boundaries occurred by the executor of the
    // CopyOutputRequest. However, the result should never contain more than
    // what was requested.
    DCHECK_LE(result->size().width(), content_rect.width());
    DCHECK_LE(result->size().height(), content_rect.height());
    media::LetterboxYUV(
        frame.get(),
        gfx::Rect(content_rect.origin(), AdjustSizeForI420(result->size())));
  } else {
    frame = nullptr;
  }

  DidCaptureFrame(frame_number, oracle_frame_number, std::move(frame),
                  content_rect);
}

void FrameSinkVideoCapturerImpl::DidCaptureFrame(
    int64_t frame_number,
    OracleFrameNumber oracle_frame_number,
    scoped_refptr<VideoFrame> frame,
    const gfx::Rect& content_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(frame_number, next_delivery_frame_number_);

  if (frame) {
    frame->metadata()->SetTimeTicks(VideoFrameMetadata::CAPTURE_END_TIME,
                                    clock_->NowTicks());
  }

  // Ensure frames are delivered in-order by using a min-heap, and only
  // deliver the next frame(s) in-sequence when they are found at the top.
  delivery_queue_.emplace(frame_number, oracle_frame_number, std::move(frame),
                          content_rect);
  while (delivery_queue_.top().frame_number == next_delivery_frame_number_) {
    auto& next = delivery_queue_.top();
    MaybeDeliverFrame(next.oracle_frame_number, std::move(next.frame),
                      next.content_rect);
    ++next_delivery_frame_number_;
    delivery_queue_.pop();
    if (delivery_queue_.empty()) {
      break;
    }
  }
}

void FrameSinkVideoCapturerImpl::MaybeDeliverFrame(
    OracleFrameNumber oracle_frame_number,
    scoped_refptr<VideoFrame> frame,
    const gfx::Rect& content_rect) {
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

  // Clone a handle to the shared memory backing the populated video frame, to
  // send to the consumer. The handle is READ_WRITE because the consumer is free
  // to modify the content further (so long as it undoes its changes before the
  // InFlightFrameDelivery::Done() call).
  DCHECK_EQ(frame->storage_type(),
            media::VideoFrame::STORAGE_MOJO_SHARED_BUFFER);
  auto* const mojo_frame =
      static_cast<media::MojoSharedBufferVideoFrame*>(frame.get());
  mojo::ScopedSharedBufferHandle buffer = mojo_frame->Handle().Clone(
      mojo::SharedBufferHandle::AccessMode::READ_WRITE);
  const uint32_t buffer_size = static_cast<uint32_t>(mojo_frame->MappedSize());

  // Assemble frame layout, format, and metadata into a mojo struct to send to
  // the consumer.
  media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
  info->timestamp = frame->timestamp();
  info->metadata = frame->metadata()->CopyInternalValues();
  info->pixel_format = frame->format();
  info->storage_type = media::VideoPixelStorage::CPU;
  info->coded_size = frame->coded_size();
  info->visible_rect = frame->visible_rect();
  const gfx::Rect update_rect = frame->visible_rect();

  // Create an InFlightFrameDelivery for this frame, owned by its mojo binding.
  // It responds to the consumer's Done() notification by returning the video
  // frame to the |frame_pool_|. It responds to the optional ProvideFeedback()
  // by forwarding the measurement to the |oracle_|.
  mojom::FrameSinkVideoConsumerFrameCallbacksPtr callbacks;
  mojo::MakeStrongBinding(
      std::make_unique<InFlightFrameDelivery>(
          frame_pool_.HoldFrameForDelivery(frame.get()),
          base::BindOnce(&VideoCaptureOracle::RecordConsumerFeedback,
                         feedback_weak_factory_.GetWeakPtr(),
                         oracle_frame_number)),
      mojo::MakeRequest(&callbacks));

  // Send the frame to the consumer.
  consumer_->OnFrameCaptured(std::move(buffer), buffer_size, std::move(info),
                             update_rect, content_rect, std::move(callbacks));
}

FrameSinkVideoCapturerImpl::CapturedFrame::CapturedFrame(
    int64_t fn,
    OracleFrameNumber ofn,
    scoped_refptr<VideoFrame> fr,
    const gfx::Rect& cr)
    : frame_number(fn),
      oracle_frame_number(ofn),
      frame(std::move(fr)),
      content_rect(cr) {}

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

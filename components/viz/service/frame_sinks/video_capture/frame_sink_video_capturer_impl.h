// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_IMPL_H_

#include <stdint.h>

#include <array>
#include <queue>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/video_capture/capturable_frame_sink.h"
#include "components/viz/service/frame_sinks/video_capture/in_flight_frame_delivery.h"
#include "components/viz/service/frame_sinks/video_capture/interprocess_frame_pool.h"
#include "components/viz/service/viz_service_export.h"
#include "media/base/video_frame.h"
#include "media/capture/content/video_capture_oracle.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace viz {

class FrameSinkVideoCapturerManager;
class FrameSinkVideoConsumer;
class CopyOutputResult;

// Captures the frames of a CompositorFrameSink's surface as a video stream.
//
// The capturer works with FrameSinkVideoCapturerManager to resolve the capture
// target, a CapturableFrameSink, from a given FrameSinkId. Once the
// target is resolved, this capturer attaches to it to receive events of
// interest regarding the frame flow, display timiming, and changes to the frame
// sink's surface. For some subset of frames, decided by
// media::VideoCaptureOracle, this capturer will make a CopyOutputRequest on the
// surface.
class VIZ_SERVICE_EXPORT FrameSinkVideoCapturerImpl final
    : public CapturableFrameSink::Client {
 public:
  // |frame_sink_manager| must outlive this instance.
  explicit FrameSinkVideoCapturerImpl(
      FrameSinkVideoCapturerManager* frame_sink_manager);

  ~FrameSinkVideoCapturerImpl() final;

  // The currently-requested frame sink for capture. The frame sink manager
  // calls this when it learns of a new CapturableFrameSink to see if the target
  // can be resolved.
  const FrameSinkId& requested_target() const { return requested_target_; }

  // Sets the resolved target, detaching this capturer from the previous target
  // (if any), and attaching to the new target. This is called by the frame sink
  // manager. If |target| is null, the capturer goes idle and expects this
  // method to be called again in the near future, once the target becomes known
  // to the frame sink manager.
  void SetResolvedTarget(CapturableFrameSink* target);

  // Notifies this capturer that the current target will be destroyed, and the
  // FrameSinkId associated with it has become invalid. This is called by the
  // frame sink manager.
  void OnTargetWillGoAway();

  // TODO(crbug.com/754872): mojom::FrameSinkVideoCapturer implementation:
  void SetFormat(media::VideoPixelFormat format, media::ColorSpace color_space);
  void SetMinCapturePeriod(base::TimeDelta min_capture_period);
  void SetResolutionConstraints(const gfx::Size& min_size,
                                const gfx::Size& max_size,
                                bool use_fixed_aspect_ratio);
  void ChangeTarget(const FrameSinkId& frame_sink_id);
  void Start(std::unique_ptr<FrameSinkVideoConsumer> consumer);
  void Stop();
  void RequestRefreshFrame();

  // Default configuration.
  static constexpr media::VideoPixelFormat kDefaultPixelFormat =
      media::PIXEL_FORMAT_I420;
  static constexpr media::ColorSpace kDefaultColorSpace =
      media::COLOR_SPACE_HD_REC709;

  // The maximum number of frames in-flight in the capture pipeline, reflecting
  // the storage capacity dedicated for this purpose. Example numbers, for a
  // frame pool that is fully-allocated with 10 frames of size 1920x1080, using
  // the I420 pixel format (12 bits per pixel). Then:
  //
  //   storage_bytes_for_all_ten_frames = 10 * (1920 * 1080 * 12/8)
  //     --> ~29.7 MB
  //
  // In practice, only 0-3 frames will be in-flight at a time, depending on the
  // content change rate and system performance.
  static constexpr int kDesignLimitMaxFrames = 10;

  // A safe, sustainable maximum number of frames in-flight. In other words,
  // exceeding 60% of the design limit is considered "red line" operation.
  static constexpr float kTargetPipelineUtilization = 0.6f;

 private:
  friend class FrameSinkVideoCapturerTest;

  using BeginFrameSourceId = decltype(BeginFrameArgs::source_id);
  using OracleFrameNumber =
      decltype(std::declval<media::VideoCaptureOracle>().next_frame_number());

  // CapturableFrameSink::Client implementation:
  void OnBeginFrame(const BeginFrameArgs& args) final;
  void OnFrameDamaged(const BeginFrameAck& ack,
                      const gfx::Size& frame_size,
                      const gfx::Rect& damage_rect) final;

  // Consults the VideoCaptureOracle to decide whether to capture a frame,
  // then ensures prerequisites are met before initiating the capture: that
  // there is a consumer present and that the pipeline is not already full.
  void MaybeCaptureFrame(media::VideoCaptureOracle::Event event,
                         const gfx::Rect& damage_rect,
                         base::TimeTicks event_time);

  // Extracts the image data from the copy output |result|, populating the
  // |content_rect| region of a [possibly letterboxed] video |frame|.
  void DidCopyFrame(int64_t frame_number,
                    OracleFrameNumber oracle_frame_number,
                    scoped_refptr<media::VideoFrame> frame,
                    const gfx::Rect& content_rect,
                    std::unique_ptr<CopyOutputResult> result);

  // Places the frame in the |delivery_queue_| and calls MaybeDeliverFrame(),
  // one frame at a time, in-order. |frame| may be null to indicate a
  // completed, but unsuccessful capture.
  void DidCaptureFrame(int64_t frame_number,
                       OracleFrameNumber oracle_frame_number,
                       scoped_refptr<media::VideoFrame> frame);

  // Delivers a |frame| to the consumer, if the VideoCaptureOracle allows
  // it. |frame| can be null to indicate a completed, but unsuccessful capture.
  // In this case, some state will be updated, but nothing will be sent to the
  // consumer.
  void MaybeDeliverFrame(OracleFrameNumber oracle_frame_number,
                         scoped_refptr<media::VideoFrame> frame);

  // Owner/Manager of this instance.
  FrameSinkVideoCapturerManager* const frame_sink_manager_;

  // Represents this instance as an issuer of CopyOutputRequests. The Surface
  // uses this to auto-cancel stale requests (i.e., prior requests that did not
  // execute). Also, the implementations that execute CopyOutputRequests use
  // this as a hint to cache objects for a huge performance improvement.
  const base::UnguessableToken copy_request_source_;

  // Use the default base::TimeTicks clock; but allow unit tests to provide a
  // replacement.
  base::DefaultTickClock default_tick_clock_;
  base::TickClock* clock_ = &default_tick_clock_;

  // Current image format.
  media::VideoPixelFormat pixel_format_ = kDefaultPixelFormat;
  media::ColorSpace color_space_ = kDefaultColorSpace;

  // Models current content change/draw behavior and proposes when to capture
  // frames, and at what size and frame rate.
  media::VideoCaptureOracle oracle_;

  // The target requested by the client, as provided in the last call to
  // ChangeTarget().
  FrameSinkId requested_target_;

  // The resolved target of video capture, or null if the requested target does
  // not yet exist (or no longer exists).
  CapturableFrameSink* resolved_target_ = nullptr;

  // The current video frame consumer. This is set when Start() is called and
  // cleared when Stop() is called.
  std::unique_ptr<FrameSinkVideoConsumer> consumer_;

  // A cache of recently-recorded future frame display times, according to the
  // BeginFrameArgs passed to OnBeginFrame() calls. This array is a ring buffer
  // mapping BeginFrameArgs::sequence_number to the expected display time.
  std::array<base::TimeTicks, kDesignLimitMaxFrames> frame_display_times_;

  // The following is used to track when the BeginFrameSource changes, to
  // determine whether the |frame_display_times_| are valid for the current
  // source.
  BeginFrameSourceId current_begin_frame_source_id_ =
      BeginFrameArgs::kStartingSourceId;

  // These are sequence counters used to ensure that the frames are being
  // delivered in the same order they are captured.
  int64_t next_capture_frame_number_ = 0;
  int64_t next_delivery_frame_number_ = 0;

  // Provides a pool of VideoFrames that can be efficiently delivered across
  // processes. The size of this pool is used to limit the maximum number of
  // frames in-flight at any one time.
  InterprocessFramePool frame_pool_;

  // A queue of captured frames pending delivery. This queue is used to re-order
  // frames, if they should happen to be captured out-of-order.
  struct CapturedFrame {
    int64_t frame_number;
    OracleFrameNumber oracle_frame_number;
    scoped_refptr<media::VideoFrame> frame;
    CapturedFrame(int64_t frame_number,
                  OracleFrameNumber oracle_frame_number,
                  scoped_refptr<media::VideoFrame> frame);
    CapturedFrame(const CapturedFrame& other);
    ~CapturedFrame();
    bool operator<(const CapturedFrame& other) const;
  };
  std::priority_queue<CapturedFrame> delivery_queue_;

  // The Oracle-provided media timestamp of the first frame. This is used to
  // compute the relative media stream timestamps for each successive frame.
  base::Optional<base::TimeTicks> first_frame_media_ticks_;

  // This class assumes its control operations and async callbacks won't execute
  // simultaneously.
  SEQUENCE_CHECKER(sequence_checker_);

  // A weak pointer factory used for cancelling consumer feedback from any
  // in-flight frame deliveries.
  base::WeakPtrFactory<media::VideoCaptureOracle> feedback_weak_factory_;

  // A weak pointer factory used for cancelling the results from any in-flight
  // copy output requests.
  base::WeakPtrFactory<FrameSinkVideoCapturerImpl> capture_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkVideoCapturerImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_IMPL_H_

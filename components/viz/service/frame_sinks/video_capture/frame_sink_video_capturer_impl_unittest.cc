// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_manager.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_consumer.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using media::VideoCaptureOracle;
using media::VideoFrame;
using media::VideoFrameMetadata;

using testing::_;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::Return;

namespace viz {
namespace {

// Dummy frame sink ID.
constexpr FrameSinkId kFrameSinkId = FrameSinkId(1, 1);

// The compositor frame interval.
constexpr base::TimeDelta kVsyncInterval =
    base::TimeDelta::FromSecondsD(1.0 / 60.0);

// The size of the compositor frame sink's Surface.
constexpr gfx::Size kSourceSize = gfx::Size(100, 100);

// The size of the VideoFrames produced by the capturer.
constexpr gfx::Size kCaptureSize = gfx::Size(32, 18);

// The location of the letterboxed content within each VideoFrame. All pixels
// outside of this region should be black.
constexpr gfx::Rect kContentRect = gfx::Rect(6, 0, 18, 18);

struct YUVColor {
  uint8_t y;
  uint8_t u;
  uint8_t v;
};

class MockFrameSinkManager : public FrameSinkVideoCapturerManager {
 public:
  MOCK_METHOD1(FindCapturableFrameSink,
               CapturableFrameSink*(const FrameSinkId& frame_sink_id));
  MOCK_METHOD1(OnCapturerConnectionLost,
               void(FrameSinkVideoCapturerImpl* capturer));
};

class MockConsumer : public FrameSinkVideoConsumer {
 public:
  MOCK_METHOD3(OnFrameCapturedMock,
               void(scoped_refptr<VideoFrame> frame,
                    const gfx::Rect& update_rect,
                    InFlightFrameDelivery* delivery));
  MOCK_METHOD1(OnTargetLost, void(const FrameSinkId& frame_sink_id));
  MOCK_METHOD0(OnStopped, void());

  int num_frames_received() const { return frames_.size(); }

  scoped_refptr<VideoFrame> TakeFrame(int i) { return std::move(frames_[i]); }

  void SendDoneNotification(int i) { std::move(done_callbacks_[i]).Run(); }

 private:
  void OnFrameCaptured(scoped_refptr<VideoFrame> frame,
                       const gfx::Rect& update_rect,
                       std::unique_ptr<InFlightFrameDelivery> delivery) final {
    ASSERT_TRUE(frame.get());
    EXPECT_EQ(gfx::Rect(kCaptureSize), update_rect);
    ASSERT_TRUE(delivery.get());
    OnFrameCapturedMock(frame, update_rect, delivery.get());
    frames_.push_back(std::move(frame));
    done_callbacks_.push_back(
        base::BindOnce(&InFlightFrameDelivery::Done, base::Passed(&delivery)));
  }

  std::vector<scoped_refptr<VideoFrame>> frames_;
  std::vector<base::OnceClosure> done_callbacks_;
};

class SolidColorI420Result : public CopyOutputResult {
 public:
  SolidColorI420Result(const gfx::Rect rect, YUVColor color)
      : CopyOutputResult(CopyOutputResult::Format::I420_PLANES, rect),
        color_(color) {}

  bool ReadI420Planes(uint8_t* y_out,
                      int y_out_stride,
                      uint8_t* u_out,
                      int u_out_stride,
                      uint8_t* v_out,
                      int v_out_stride) const final {
    CHECK(y_out);
    CHECK(y_out_stride >= size().width());
    CHECK(u_out);
    const int chroma_width = (size().width() + 1) / 2;
    CHECK(u_out_stride >= chroma_width);
    CHECK(v_out);
    CHECK(v_out_stride >= chroma_width);
    for (int i = 0; i < size().height(); ++i, y_out += y_out_stride) {
      memset(y_out, color_.y, size().width());
    }
    const int chroma_height = (size().height() + 1) / 2;
    for (int i = 0; i < chroma_height; ++i, u_out += u_out_stride) {
      memset(u_out, color_.u, chroma_width);
    }
    for (int i = 0; i < chroma_height; ++i, v_out += v_out_stride) {
      memset(v_out, color_.v, chroma_width);
    }
    return true;
  }

 private:
  const YUVColor color_;
};

class FakeCapturableFrameSink : public CapturableFrameSink {
 public:
  Client* attached_client() const { return client_; }

  void AttachCaptureClient(Client* client) override {
    ASSERT_FALSE(client_);
    ASSERT_TRUE(client);
    client_ = client;
  }

  void DetachCaptureClient(Client* client) override {
    ASSERT_TRUE(client);
    ASSERT_EQ(client, client_);
    client_ = nullptr;
  }

  gfx::Size GetSurfaceSize() override { return kSourceSize; }

  void RequestCopyOfSurface(
      std::unique_ptr<CopyOutputRequest> request) override {
    EXPECT_EQ(CopyOutputResult::Format::I420_PLANES, request->result_format());
    EXPECT_NE(base::UnguessableToken(), request->source());
    EXPECT_EQ(gfx::Rect(kSourceSize), request->area());
    EXPECT_EQ(gfx::Rect(kContentRect.size()), request->result_selection());

    auto result = std::make_unique<SolidColorI420Result>(
        request->result_selection(), color_);
    results_.push_back(base::BindOnce(
        [](std::unique_ptr<CopyOutputRequest> request,
           std::unique_ptr<CopyOutputResult> result) {
          request->SendResult(std::move(result));
        },
        base::Passed(&request), base::Passed(&result)));
  }

  void SetCopyOutputColor(YUVColor color) { color_ = color; }

  int num_copy_results() const { return results_.size(); }

  void SendCopyOutputResult(int offset) {
    auto it = results_.begin() + offset;
    std::move(*it).Run();
  }

 private:
  CapturableFrameSink::Client* client_ = nullptr;
  YUVColor color_ = {0xde, 0xad, 0xbf};

  std::vector<base::OnceClosure> results_;
};

// Matcher that returns true if the content region of a letterboxed VideoFrame
// is filled with the given color, and black everywhere else.
MATCHER_P(IsLetterboxedFrame, color, "") {
  if (!arg) {
    return false;
  }

  const VideoFrame& frame = *arg;
  const auto IsLetterboxedPlane = [&frame](int plane, uint8_t color) {
    gfx::Rect content_rect = kContentRect;
    if (plane != VideoFrame::kYPlane) {
      content_rect =
          gfx::Rect(content_rect.x() / 2, content_rect.y() / 2,
                    content_rect.width() / 2, content_rect.height() / 2);
    }
    for (int row = 0; row < frame.rows(plane); ++row) {
      const uint8_t* p = frame.visible_data(plane) + row * frame.stride(plane);
      for (int col = 0; col < frame.row_bytes(plane); ++col) {
        if (content_rect.Contains(gfx::Point(col, row))) {
          if (p[col] != color) {
            return false;
          }
        } else {  // Letterbox border around content.
          if (plane == VideoFrame::kYPlane && p[col] != 0x00) {
            return false;
          }
        }
      }
    }
    return true;
  };

  return IsLetterboxedPlane(VideoFrame::kYPlane, color.y) &&
         IsLetterboxedPlane(VideoFrame::kUPlane, color.u) &&
         IsLetterboxedPlane(VideoFrame::kVPlane, color.v);
}

}  // namespace

class FrameSinkVideoCapturerTest : public testing::Test {
 public:
  FrameSinkVideoCapturerTest() : capturer_(&frame_sink_manager_) {}

  void SetUp() override {
    // Override the capturer's TickClock with the one controlled by the tests.
    start_time_ = base::TimeTicks() + base::TimeDelta::FromSeconds(1);
    clock_.SetNowTicks(start_time_);
    capturer_.clock_ = &clock_;

    // Before setting the format, ensure the defaults are in-place. Then, for
    // these tests, set a specific format and color space.
    ASSERT_EQ(FrameSinkVideoCapturerImpl::kDefaultPixelFormat,
              capturer_.pixel_format_);
    ASSERT_EQ(FrameSinkVideoCapturerImpl::kDefaultColorSpace,
              capturer_.color_space_);
    capturer_.SetFormat(media::PIXEL_FORMAT_I420, media::COLOR_SPACE_HD_REC709);
    ASSERT_EQ(media::PIXEL_FORMAT_I420, capturer_.pixel_format_);
    ASSERT_EQ(media::COLOR_SPACE_HD_REC709, capturer_.color_space_);

    // Set min capture period as small as possible so that the
    // media::VideoCapturerOracle used by the capturer will want to capture
    // every composited frame. The capturer will override the too-small value of
    // zero with a value based on media::limits::kMaxFramesPerSecond.
    capturer_.SetMinCapturePeriod(base::TimeDelta());
    ASSERT_LT(base::TimeDelta(), capturer_.oracle_.min_capture_period());

    capturer_.SetResolutionConstraints(kCaptureSize, kCaptureSize, false);
  }

  void AdvanceClockToNextVsync() {
    const auto num_vsyncs_elapsed =
        (clock_.NowTicks() - start_time_) / kVsyncInterval;
    const auto advance_to_time =
        start_time_ + (num_vsyncs_elapsed + 1) * kVsyncInterval;
    clock_.Advance(advance_to_time - clock_.NowTicks());
  }

  void NotifyBeginFrame(int frame_number) {
    BeginFrameArgs args;
    args.interval = kVsyncInterval;
    args.frame_time = clock_.NowTicks();
    args.sequence_number = BeginFrameArgs::kStartingFrameNumber + frame_number;
    args.source_id = 1;
    capturer_.OnBeginFrame(args);
  }

  void NotifyFrameDamaged(int frame_number) {
    BeginFrameAck ack;
    ack.sequence_number = BeginFrameArgs::kStartingFrameNumber + frame_number;
    ack.source_id = 1;
    capturer_.OnFrameDamaged(ack, kSourceSize, gfx::Rect(kSourceSize));
  }

 protected:
  base::TimeTicks start_time_;
  base::SimpleTestTickClock clock_;
  MockFrameSinkManager frame_sink_manager_;
  FakeCapturableFrameSink frame_sink_;
  FrameSinkVideoCapturerImpl capturer_;
};

// Tests that the capturer attaches to a frame sink immediately, in the case
// where the frame sink was already known by the manger.
TEST_F(FrameSinkVideoCapturerTest, ResolvesTargetImmediately) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  EXPECT_EQ(FrameSinkId(), capturer_.requested_target());
  capturer_.ChangeTarget(kFrameSinkId);
  EXPECT_EQ(kFrameSinkId, capturer_.requested_target());
  EXPECT_EQ(&capturer_, frame_sink_.attached_client());
}

// Tests that the capturer attaches to a frame sink later, in the case where the
// frame sink becomes known to the manager at some later point.
TEST_F(FrameSinkVideoCapturerTest, ResolvesTargetLater) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(nullptr));

  EXPECT_EQ(FrameSinkId(), capturer_.requested_target());
  capturer_.ChangeTarget(kFrameSinkId);
  EXPECT_EQ(kFrameSinkId, capturer_.requested_target());
  EXPECT_EQ(nullptr, frame_sink_.attached_client());

  capturer_.SetResolvedTarget(&frame_sink_);
  EXPECT_EQ(&capturer_, frame_sink_.attached_client());
}

// Tests that the capturer reports a lost target to the consumer. The consumer
// may then change targets to capture something else.
TEST_F(FrameSinkVideoCapturerTest, ReportsTargetLost) {
  FakeCapturableFrameSink prior_frame_sink;
  constexpr FrameSinkId kPriorFrameSinkId = FrameSinkId(1, 2);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kPriorFrameSinkId))
      .WillOnce(Return(&prior_frame_sink));
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillOnce(Return(&frame_sink_));

  auto consumer = std::make_unique<NiceMock<MockConsumer>>();
  EXPECT_CALL(*consumer, OnTargetLost(kPriorFrameSinkId)).Times(1);
  capturer_.Start(std::move(consumer));

  capturer_.ChangeTarget(kPriorFrameSinkId);
  EXPECT_EQ(kPriorFrameSinkId, capturer_.requested_target());
  EXPECT_EQ(&capturer_, prior_frame_sink.attached_client());
  EXPECT_EQ(nullptr, frame_sink_.attached_client());

  capturer_.OnTargetWillGoAway();
  EXPECT_EQ(nullptr, prior_frame_sink.attached_client());
  EXPECT_EQ(nullptr, frame_sink_.attached_client());

  capturer_.ChangeTarget(kFrameSinkId);
  EXPECT_EQ(kFrameSinkId, capturer_.requested_target());
  EXPECT_EQ(nullptr, prior_frame_sink.attached_client());
  EXPECT_EQ(&capturer_, frame_sink_.attached_client());

  capturer_.Stop();
}

// Tests that an initial black frame is sent, in the case where a target is not
// resolved at the time Start() is called.
TEST_F(FrameSinkVideoCapturerTest, SendsBlackFrameOnStartWithoutATarget) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  auto consumer = std::make_unique<MockConsumer>();
  EXPECT_CALL(
      *consumer,
      OnFrameCapturedMock(IsLetterboxedFrame(YUVColor{0x00, 0x80, 0x80}), _, _))
      .Times(1);
  EXPECT_CALL(*consumer, OnTargetLost(kFrameSinkId)).Times(0);
  EXPECT_CALL(*consumer, OnStopped()).Times(1);

  capturer_.Start(std::move(consumer));
  // A copy request was not necessary.
  EXPECT_EQ(0, frame_sink_.num_copy_results());
  capturer_.Stop();
}

// An end-to-end pipeline test where compositor updates trigger the capturer to
// make copy requests, and a stream of video frames is delivered to the
// consumer.
TEST_F(FrameSinkVideoCapturerTest, CapturesCompositedFrames) {
  frame_sink_.SetCopyOutputColor(YUVColor{0x80, 0x80, 0x80});
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_.ChangeTarget(kFrameSinkId);

  MockConsumer* consumer;
  {
    auto mock_consumer = std::make_unique<MockConsumer>();
    consumer = mock_consumer.get();
    capturer_.Start(std::move(mock_consumer));
  }
  const int num_refresh_frames = 1;
  const int num_update_frames =
      3 * FrameSinkVideoCapturerImpl::kDesignLimitMaxFrames;
  EXPECT_CALL(*consumer, OnFrameCapturedMock(_, _, _))
      .Times(num_refresh_frames + num_update_frames);
  EXPECT_CALL(*consumer, OnTargetLost(_)).Times(0);
  EXPECT_CALL(*consumer, OnStopped()).Times(1);

  // To start, the capturer will make a copy request for the initial refresh
  // frame. Simulate a copy result and expect to see the refresh frame delivered
  // to the consumer.
  ASSERT_EQ(num_refresh_frames, frame_sink_.num_copy_results());
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(num_refresh_frames, consumer->num_frames_received());
  EXPECT_THAT(consumer->TakeFrame(0),
              IsLetterboxedFrame(YUVColor{0x80, 0x80, 0x80}));
  consumer->SendDoneNotification(0);

  // Drive the capturer pipeline for a series of frame composites.
  base::TimeDelta last_timestamp;
  for (int i = num_refresh_frames; i < num_refresh_frames + num_update_frames;
       ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);

    // Move time forward to the next display vsync and notify the capturer that
    // compositing of the frame has begun.
    AdvanceClockToNextVsync();
    const base::TimeTicks expected_reference_time =
        clock_.NowTicks() + kVsyncInterval;
    NotifyBeginFrame(i);

    // Change the content of the frame sink and notify the capturer of the
    // damage.
    const YUVColor color = {i << 4, (i << 4) + 0x10, (i << 4) + 0x20};
    frame_sink_.SetCopyOutputColor(color);
    clock_.Advance(kVsyncInterval / 4);
    const base::TimeTicks expected_capture_begin_time = clock_.NowTicks();
    NotifyFrameDamaged(i);

    // The frame sink should have received a CopyOutputRequest. Simulate a short
    // pause before the result is sent back to the capturer, and the capturer
    // should then deliver the frame.
    ASSERT_EQ(i + 1, frame_sink_.num_copy_results());
    clock_.Advance(kVsyncInterval / 4);
    const base::TimeTicks expected_capture_end_time = clock_.NowTicks();
    frame_sink_.SendCopyOutputResult(i);
    ASSERT_EQ(i + 1, consumer->num_frames_received());

    // Verify the frame is the right size, has the right content, and has
    // required metadata set.
    const scoped_refptr<VideoFrame> frame = consumer->TakeFrame(i);
    EXPECT_THAT(frame, IsLetterboxedFrame(color));
    EXPECT_EQ(kCaptureSize, frame->coded_size());
    EXPECT_EQ(gfx::Rect(kCaptureSize), frame->visible_rect());
    EXPECT_EQ(gfx::ColorSpace::CreateREC709(), frame->ColorSpace());
    EXPECT_LT(last_timestamp, frame->timestamp());
    last_timestamp = frame->timestamp();
    const VideoFrameMetadata* metadata = frame->metadata();
    base::TimeTicks capture_begin_time;
    EXPECT_TRUE(metadata->GetTimeTicks(VideoFrameMetadata::CAPTURE_BEGIN_TIME,
                                       &capture_begin_time));
    EXPECT_EQ(expected_capture_begin_time, capture_begin_time);
    base::TimeTicks capture_end_time;
    EXPECT_TRUE(metadata->GetTimeTicks(VideoFrameMetadata::CAPTURE_END_TIME,
                                       &capture_end_time));
    EXPECT_EQ(expected_capture_end_time, capture_end_time);
    int color_space = media::COLOR_SPACE_UNSPECIFIED;
    EXPECT_TRUE(
        metadata->GetInteger(VideoFrameMetadata::COLOR_SPACE, &color_space));
    EXPECT_EQ(media::COLOR_SPACE_HD_REC709, color_space);
    EXPECT_TRUE(metadata->HasKey(VideoFrameMetadata::FRAME_DURATION));
    // FRAME_DURATION is an estimate computed by the VideoCaptureOracle, so it
    // its exact value is not being checked here.
    double frame_rate = 0.0;
    EXPECT_TRUE(
        metadata->GetDouble(VideoFrameMetadata::FRAME_RATE, &frame_rate));
    EXPECT_NEAR(media::limits::kMaxFramesPerSecond, frame_rate, 0.001);
    base::TimeTicks reference_time;
    EXPECT_TRUE(metadata->GetTimeTicks(VideoFrameMetadata::REFERENCE_TIME,
                                       &reference_time));
    EXPECT_EQ(expected_reference_time, reference_time);

    // Notify the capturer that the consumer is done with the frame.
    consumer->SendDoneNotification(i);

    if (HasFailure()) {
      break;
    }
  }

  capturer_.Stop();
}

// Tests that frame capturing halts when too many frames are in-flight, whether
// that is because there are too many copy requests in-flight or because the
// consumer has not finished consuming frames fast enough.
TEST_F(FrameSinkVideoCapturerTest, HaltsWhenPipelineIsFull) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_.ChangeTarget(kFrameSinkId);

  MockConsumer* consumer;
  {
    auto mock_consumer = std::make_unique<NiceMock<MockConsumer>>();
    consumer = mock_consumer.get();
    capturer_.Start(std::move(mock_consumer));
  }

  // Saturate the pipeline with CopyOutputRequests that have not yet executed.
  const int num_refresh_frames = 1;
  int num_frames = FrameSinkVideoCapturerImpl::kDesignLimitMaxFrames;
  for (int i = num_refresh_frames; i < num_frames; ++i) {
    AdvanceClockToNextVsync();
    NotifyBeginFrame(i);
    NotifyFrameDamaged(i);
  }
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());

  // Notifying the capturer of new compositor updates should cause no new copy
  // requests to be issued at this point.
  const int first_uncaptured_frame = num_frames;
  AdvanceClockToNextVsync();
  NotifyBeginFrame(first_uncaptured_frame);
  NotifyFrameDamaged(first_uncaptured_frame);
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());

  // Complete the first copy request. When notifying the capturer of another
  // compositor update, no new copy requests should be issued because the first
  // frame is still in the middle of being delivered/consumed.
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(1, consumer->num_frames_received());
  const int second_uncaptured_frame = num_frames;
  AdvanceClockToNextVsync();
  NotifyBeginFrame(second_uncaptured_frame);
  NotifyFrameDamaged(second_uncaptured_frame);
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());

  // Notify the capturer that the first frame has been consumed. Then, with
  // another compositor update, the capturer should issue another new copy
  // request.
  EXPECT_TRUE(consumer->TakeFrame(0));
  consumer->SendDoneNotification(0);
  const int first_capture_resumed_frame = second_uncaptured_frame + 1;
  AdvanceClockToNextVsync();
  NotifyBeginFrame(first_capture_resumed_frame);
  NotifyFrameDamaged(first_capture_resumed_frame);
  ++num_frames;
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());

  // With yet another compositor update, no new copy requests should be issued
  // because the pipeline became saturated again.
  const int third_uncaptured_frame = first_capture_resumed_frame + 1;
  AdvanceClockToNextVsync();
  NotifyBeginFrame(third_uncaptured_frame);
  NotifyFrameDamaged(third_uncaptured_frame);
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());

  // Complete all pending copy requests. Another compositor update should not
  // cause any new copy requests to be issued because all frames are being
  // delivered/consumed.
  for (int i = 1; i < frame_sink_.num_copy_results(); ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);
    frame_sink_.SendCopyOutputResult(i);
  }
  ASSERT_EQ(frame_sink_.num_copy_results(), consumer->num_frames_received());
  const int fourth_uncaptured_frame = third_uncaptured_frame + 1;
  AdvanceClockToNextVsync();
  NotifyBeginFrame(fourth_uncaptured_frame);
  NotifyFrameDamaged(fourth_uncaptured_frame);
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());

  // Notify the capturer that all frames have been consumed. Finally, with
  // another compositor update, capture should resume.
  for (int i = 1; i < consumer->num_frames_received(); ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);
    EXPECT_TRUE(consumer->TakeFrame(i));
    consumer->SendDoneNotification(i);
  }
  const int second_capture_resumed_frame = fourth_uncaptured_frame + 1;
  AdvanceClockToNextVsync();
  NotifyBeginFrame(second_capture_resumed_frame);
  NotifyFrameDamaged(second_capture_resumed_frame);
  ++num_frames;
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  frame_sink_.SendCopyOutputResult(frame_sink_.num_copy_results() - 1);
  ASSERT_EQ(frame_sink_.num_copy_results(), consumer->num_frames_received());

  capturer_.Stop();
}

// Tests that copy requests completed out-of-order are accounted for by the
// capturer, with results delivered to the consumer in-order.
TEST_F(FrameSinkVideoCapturerTest, DeliversFramesInOrder) {
  std::vector<YUVColor> colors;
  colors.push_back(YUVColor{0x00, 0x80, 0x80});
  frame_sink_.SetCopyOutputColor(colors.back());
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_.ChangeTarget(kFrameSinkId);

  MockConsumer* consumer;
  {
    auto mock_consumer = std::make_unique<NiceMock<MockConsumer>>();
    consumer = mock_consumer.get();
    capturer_.Start(std::move(mock_consumer));
  }

  // Issue five CopyOutputRequests (1 refresh frame plus 4 compositor
  // updates). Each composited frame has its content region set to a different
  // color to check that the video frames are being delivered in-order.
  const int num_refresh_frames = 1;
  int num_frames = 5;
  ASSERT_EQ(num_refresh_frames, frame_sink_.num_copy_results());
  for (int i = num_refresh_frames; i < num_frames; ++i) {
    colors.push_back(YUVColor{static_cast<uint8_t>(i << 4),
                              static_cast<uint8_t>((i << 4) + 0x10),
                              static_cast<uint8_t>((i << 4) + 0x20)});
    frame_sink_.SetCopyOutputColor(colors.back());
    AdvanceClockToNextVsync();
    NotifyBeginFrame(i);
    NotifyFrameDamaged(i);
  }
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());

  // Complete the copy requests out-of-order. Check that frames are not
  // delivered until they can all be delivered in-order, and that the content of
  // each video frame is correct.
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(1, consumer->num_frames_received());
  EXPECT_THAT(consumer->TakeFrame(0), IsLetterboxedFrame(colors[0]));
  frame_sink_.SendCopyOutputResult(2);
  ASSERT_EQ(1, consumer->num_frames_received());  // Waiting for frame 1.
  frame_sink_.SendCopyOutputResult(3);
  ASSERT_EQ(1, consumer->num_frames_received());  // Still waiting for frame 1.
  frame_sink_.SendCopyOutputResult(1);
  ASSERT_EQ(4, consumer->num_frames_received());  // Sent frames 1, 2, and 3.
  EXPECT_THAT(consumer->TakeFrame(1), IsLetterboxedFrame(colors[1]));
  EXPECT_THAT(consumer->TakeFrame(2), IsLetterboxedFrame(colors[2]));
  EXPECT_THAT(consumer->TakeFrame(3), IsLetterboxedFrame(colors[3]));
  frame_sink_.SendCopyOutputResult(4);
  ASSERT_EQ(5, consumer->num_frames_received());
  EXPECT_THAT(consumer->TakeFrame(4), IsLetterboxedFrame(colors[4]));

  capturer_.Stop();
}

// Tests that in-flight copy requests are canceled when the capturer is
// stopped. When it is started again with a new consumer, only the results from
// newer copy requests should appear in video frames delivered to the consumer.
TEST_F(FrameSinkVideoCapturerTest, CancelsInFlightCapturesOnStop) {
  const YUVColor color1 = {0xaa, 0xbb, 0xcc};
  frame_sink_.SetCopyOutputColor(color1);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_.ChangeTarget(kFrameSinkId);

  // Start capturing to the first consumer.
  MockConsumer* consumer;
  {
    auto mock_consumer = std::make_unique<MockConsumer>();
    consumer = mock_consumer.get();
    capturer_.Start(std::move(mock_consumer));
  }
  EXPECT_CALL(*consumer, OnFrameCapturedMock(_, _, _)).Times(2);
  EXPECT_CALL(*consumer, OnTargetLost(_)).Times(0);
  EXPECT_CALL(*consumer, OnStopped()).Times(1);

  // Issue three additional CopyOutputRequests. With the initial refresh frame,
  // the total should be four.
  int num_refresh_frames = 1;
  ASSERT_EQ(num_refresh_frames, frame_sink_.num_copy_results());
  int num_copy_requests = 4;
  for (int i = num_refresh_frames; i < num_copy_requests; ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);
    AdvanceClockToNextVsync();
    NotifyBeginFrame(i);
    NotifyFrameDamaged(i);
  }
  ASSERT_EQ(num_copy_requests, frame_sink_.num_copy_results());

  // Complete the first two copy requests.
  int num_completed_captures = 2;
  for (int i = 0; i < num_completed_captures; ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);
    frame_sink_.SendCopyOutputResult(i);
    ASSERT_EQ(i + 1, consumer->num_frames_received());
    EXPECT_THAT(consumer->TakeFrame(i), IsLetterboxedFrame(color1));
  }

  // Stopping capture should cancel the remaning copy requests.
  capturer_.Stop();

  // Change the content color and start capturing to the second consumer.
  const YUVColor color2 = {0xdd, 0xee, 0xff};
  frame_sink_.SetCopyOutputColor(color2);
  MockConsumer* consumer2;
  {
    auto mock_consumer2 = std::make_unique<MockConsumer>();
    consumer2 = mock_consumer2.get();
    capturer_.Start(std::move(mock_consumer2));
  }
  const int num_captures_for_second_consumer = 3;
  EXPECT_CALL(*consumer2, OnFrameCapturedMock(_, _, _))
      .Times(num_captures_for_second_consumer);
  EXPECT_CALL(*consumer2, OnTargetLost(_)).Times(0);
  EXPECT_CALL(*consumer2, OnStopped()).Times(1);

  // Complete the copy requests for the first consumer. Expect that they have no
  // effect on the second consumer.
  for (int i = num_completed_captures; i < num_copy_requests; ++i) {
    frame_sink_.SendCopyOutputResult(i);
    ASSERT_EQ(0, consumer2->num_frames_received());
  }
  num_completed_captures = 0;

  // Note: Because the clock hasn't advanced while switching consumers, the
  // capturer won't send a refresh frame. This is because the VideoCaptureOracle
  // thinks the frame rate would be too fast.
  //
  // TODO(crbug.com/785072): Revisit this because the capturer should always
  // guarantee an initial video frame is sent to the consumer within a
  // reasonable time period. In practice, it does; but if things happen too
  // quickly, it might not.
  num_refresh_frames = 0;

  // From here, any new copy requests should be executed with video frames
  // delivered to the consumer containing |color2|.
  for (int i = num_refresh_frames; i < num_captures_for_second_consumer; ++i) {
    AdvanceClockToNextVsync();
    NotifyBeginFrame(num_copy_requests);
    NotifyFrameDamaged(num_copy_requests);
    ++num_copy_requests;
    ASSERT_EQ(num_copy_requests, frame_sink_.num_copy_results());
    frame_sink_.SendCopyOutputResult(frame_sink_.num_copy_results() - 1);
    ++num_completed_captures;
    ASSERT_EQ(num_completed_captures, consumer2->num_frames_received());
    EXPECT_THAT(consumer2->TakeFrame(consumer2->num_frames_received() - 1),
                IsLetterboxedFrame(color2));
  }

  capturer_.Stop();
}

}  // namespace viz

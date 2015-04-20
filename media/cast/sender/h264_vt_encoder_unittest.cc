// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_suite.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/cast/sender/h264_vt_encoder.h"
#include "media/cast/sender/video_frame_factory.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kVideoWidth = 1280;
const int kVideoHeight = 720;

class MediaTestSuite : public base::TestSuite {
 public:
  MediaTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}
  ~MediaTestSuite() override {}

 protected:
  void Initialize() override;
};

void MediaTestSuite::Initialize() {
  base::TestSuite::Initialize();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kEnableInbandTextTracks);
  media::InitializeMediaLibraryForTesting();
}

}  // namespace

int main(int argc, char** argv) {
  {
    base::AtExitManager at_exit_manager;
    CHECK(VideoToolboxGlue::Get())
        << "VideoToolbox is not available. Requires OS X 10.8 or iOS 8.0.";
  }
  MediaTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&MediaTestSuite::Run, base::Unretained(&test_suite)));
}

namespace media {
namespace cast {

// See comment in end2end_unittest.cc for details on this value.
const double kVideoAcceptedPSNR = 38.0;

void SavePipelineStatus(PipelineStatus* out_status, PipelineStatus in_status) {
  *out_status = in_status;
}

void SaveOperationalStatus(OperationalStatus* out_status,
                           OperationalStatus in_status) {
  *out_status = in_status;
}

class MetadataRecorder : public base::RefCountedThreadSafe<MetadataRecorder> {
 public:
  MetadataRecorder() : count_frames_delivered_(0) {}

  int count_frames_delivered() const { return count_frames_delivered_; }

  void PushExpectation(uint32 expected_frame_id,
                       uint32 expected_last_referenced_frame_id,
                       uint32 expected_rtp_timestamp,
                       const base::TimeTicks& expected_reference_time) {
    expectations_.push(Expectation{expected_frame_id,
                                   expected_last_referenced_frame_id,
                                   expected_rtp_timestamp,
                                   expected_reference_time});
  }

  void CompareFrameWithExpected(scoped_ptr<EncodedFrame> encoded_frame) {
    ASSERT_LT(0u, expectations_.size());
    auto e = expectations_.front();
    expectations_.pop();
    if (e.expected_frame_id != e.expected_last_referenced_frame_id) {
      EXPECT_EQ(EncodedFrame::DEPENDENT, encoded_frame->dependency);
    } else {
      EXPECT_EQ(EncodedFrame::KEY, encoded_frame->dependency);
    }
    EXPECT_EQ(e.expected_frame_id, encoded_frame->frame_id);
    EXPECT_EQ(e.expected_last_referenced_frame_id,
              encoded_frame->referenced_frame_id)
        << "frame id: " << e.expected_frame_id;
    EXPECT_EQ(e.expected_rtp_timestamp, encoded_frame->rtp_timestamp);
    EXPECT_EQ(e.expected_reference_time, encoded_frame->reference_time);
    EXPECT_FALSE(encoded_frame->data.empty());
    ++count_frames_delivered_;
  }

 private:
  friend class base::RefCountedThreadSafe<MetadataRecorder>;
  virtual ~MetadataRecorder() {}

  int count_frames_delivered_;

  struct Expectation {
    uint32 expected_frame_id;
    uint32 expected_last_referenced_frame_id;
    uint32 expected_rtp_timestamp;
    base::TimeTicks expected_reference_time;
  };
  std::queue<Expectation> expectations_;

  DISALLOW_COPY_AND_ASSIGN(MetadataRecorder);
};

class EndToEndFrameChecker
    : public base::RefCountedThreadSafe<EndToEndFrameChecker> {
 public:
  explicit EndToEndFrameChecker(const VideoDecoderConfig& config)
      : decoder_(base::MessageLoop::current()->task_runner()),
        count_frames_checked_(0) {
    PipelineStatus pipeline_status;
    decoder_.Initialize(
        config, false, base::Bind(&SavePipelineStatus, &pipeline_status),
        base::Bind(&EndToEndFrameChecker::CompareFrameWithExpected,
                   base::Unretained(this)));
    base::MessageLoop::current()->RunUntilIdle();
    EXPECT_EQ(PIPELINE_OK, pipeline_status);
  }

  void PushExpectation(const scoped_refptr<VideoFrame>& frame) {
    expectations_.push(frame);
  }

  void EncodeDone(scoped_ptr<EncodedFrame> encoded_frame) {
    auto buffer = DecoderBuffer::CopyFrom(encoded_frame->bytes(),
                                          encoded_frame->data.size());
    decoder_.Decode(buffer, base::Bind(&EndToEndFrameChecker::DecodeDone,
                                       base::Unretained(this)));
  }

  void CompareFrameWithExpected(const scoped_refptr<VideoFrame>& frame) {
    ASSERT_LT(0u, expectations_.size());
    auto& e = expectations_.front();
    expectations_.pop();
    EXPECT_LE(kVideoAcceptedPSNR, I420PSNR(e, frame));
    ++count_frames_checked_;
  }

  void DecodeDone(VideoDecoder::Status status) {
    EXPECT_EQ(VideoDecoder::kOk, status);
  }

  int count_frames_checked() const { return count_frames_checked_; }

 private:
  friend class base::RefCountedThreadSafe<EndToEndFrameChecker>;
  virtual ~EndToEndFrameChecker() {}

  FFmpegVideoDecoder decoder_;
  std::queue<scoped_refptr<VideoFrame>> expectations_;
  int count_frames_checked_;

  DISALLOW_COPY_AND_ASSIGN(EndToEndFrameChecker);
};

void CreateFrameAndMemsetPlane(VideoFrameFactory* const video_frame_factory) {
  const scoped_refptr<media::VideoFrame> video_frame =
      video_frame_factory->MaybeCreateFrame(
          gfx::Size(kVideoWidth, kVideoHeight), base::TimeDelta());
  ASSERT_TRUE(video_frame.get());
  auto cv_pixel_buffer = video_frame->cv_pixel_buffer();
  ASSERT_TRUE(cv_pixel_buffer);
  CVPixelBufferLockBaseAddress(cv_pixel_buffer, 0);
  auto ptr = CVPixelBufferGetBaseAddressOfPlane(cv_pixel_buffer, 0);
  ASSERT_TRUE(ptr);
  memset(ptr, 0xfe, CVPixelBufferGetBytesPerRowOfPlane(cv_pixel_buffer, 0) *
                        CVPixelBufferGetHeightOfPlane(cv_pixel_buffer, 0));
  CVPixelBufferUnlockBaseAddress(cv_pixel_buffer, 0);
}

class H264VideoToolboxEncoderTest : public ::testing::Test {
 protected:
  H264VideoToolboxEncoderTest()
      : operational_status_(STATUS_UNINITIALIZED) {
    frame_->set_timestamp(base::TimeDelta());
  }

  void SetUp() override {
    clock_ = new base::SimpleTestTickClock();
    clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());

    cast_environment_ = new CastEnvironment(
        scoped_ptr<base::TickClock>(clock_).Pass(),
        message_loop_.message_loop_proxy(), message_loop_.message_loop_proxy(),
        message_loop_.message_loop_proxy());
    encoder_.reset(new H264VideoToolboxEncoder(
        cast_environment_,
        video_sender_config_,
        gfx::Size(kVideoWidth, kVideoHeight),
        0u,
        base::Bind(&SaveOperationalStatus, &operational_status_)));
    message_loop_.RunUntilIdle();
    EXPECT_EQ(STATUS_INITIALIZED, operational_status_);
  }

  void TearDown() override {
    encoder_.reset();
    message_loop_.RunUntilIdle();
  }

  void AdvanceClockAndVideoFrameTimestamp() {
    clock_->Advance(base::TimeDelta::FromMilliseconds(33));
    frame_->set_timestamp(frame_->timestamp() +
                          base::TimeDelta::FromMilliseconds(33));
  }

  static void SetUpTestCase() {
    // Reusable test data.
    video_sender_config_ = GetDefaultVideoSenderConfig();
    video_sender_config_.codec = CODEC_VIDEO_H264;
    const gfx::Size size(kVideoWidth, kVideoHeight);
    frame_ = media::VideoFrame::CreateFrame(
        VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(frame_.get(), 123);
  }

  static void TearDownTestCase() { frame_ = nullptr; }

  static scoped_refptr<media::VideoFrame> frame_;
  static VideoSenderConfig video_sender_config_;

  base::SimpleTestTickClock* clock_;  // Owned by CastEnvironment.
  base::MessageLoop message_loop_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<VideoEncoder> encoder_;
  OperationalStatus operational_status_;

 private:
  DISALLOW_COPY_AND_ASSIGN(H264VideoToolboxEncoderTest);
};

// static
scoped_refptr<media::VideoFrame> H264VideoToolboxEncoderTest::frame_;
VideoSenderConfig H264VideoToolboxEncoderTest::video_sender_config_;

TEST_F(H264VideoToolboxEncoderTest, CheckFrameMetadataSequence) {
  scoped_refptr<MetadataRecorder> metadata_recorder(new MetadataRecorder());
  VideoEncoder::FrameEncodedCallback cb = base::Bind(
      &MetadataRecorder::CompareFrameWithExpected, metadata_recorder.get());

  metadata_recorder->PushExpectation(
      0, 0, TimeDeltaToRtpDelta(frame_->timestamp(), kVideoFrequency),
      clock_->NowTicks());
  EXPECT_TRUE(encoder_->EncodeVideoFrame(frame_, clock_->NowTicks(), cb));
  message_loop_.RunUntilIdle();

  for (uint32 frame_id = 1; frame_id < 10; ++frame_id) {
    AdvanceClockAndVideoFrameTimestamp();
    metadata_recorder->PushExpectation(
        frame_id, frame_id - 1,
        TimeDeltaToRtpDelta(frame_->timestamp(), kVideoFrequency),
        clock_->NowTicks());
    EXPECT_TRUE(encoder_->EncodeVideoFrame(frame_, clock_->NowTicks(), cb));
  }

  encoder_.reset();
  message_loop_.RunUntilIdle();

  EXPECT_EQ(10, metadata_recorder->count_frames_delivered());
}

#if defined(USE_PROPRIETARY_CODECS)
TEST_F(H264VideoToolboxEncoderTest, CheckFramesAreDecodable) {
  VideoDecoderConfig config(kCodecH264, H264PROFILE_MAIN, frame_->format(),
                            frame_->coded_size(), frame_->visible_rect(),
                            frame_->natural_size(), nullptr, 0, false);
  scoped_refptr<EndToEndFrameChecker> checker(new EndToEndFrameChecker(config));

  VideoEncoder::FrameEncodedCallback cb =
      base::Bind(&EndToEndFrameChecker::EncodeDone, checker.get());
  for (uint32 frame_id = 0; frame_id < 6; ++frame_id) {
    checker->PushExpectation(frame_);
    EXPECT_TRUE(encoder_->EncodeVideoFrame(frame_, clock_->NowTicks(), cb));
    AdvanceClockAndVideoFrameTimestamp();
  }

  encoder_.reset();
  message_loop_.RunUntilIdle();

  EXPECT_EQ(5, checker->count_frames_checked());
}
#endif

TEST_F(H264VideoToolboxEncoderTest, CheckVideoFrameFactory) {
  auto video_frame_factory = encoder_->CreateVideoFrameFactory();
  ASSERT_TRUE(video_frame_factory.get());
  CreateFrameAndMemsetPlane(video_frame_factory.get());
  // TODO(jfroy): Need to test that the encoder can encode VideoFrames provided
  // by the VideoFrameFactory.
  encoder_.reset();
  message_loop_.RunUntilIdle();
  CreateFrameAndMemsetPlane(video_frame_factory.get());
}

}  // namespace cast
}  // namespace media

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/sender/external_video_encoder.h"
#include "media/cast/sender/fake_video_encode_accelerator_factory.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

namespace {

class TestVideoEncoderCallback
    : public base::RefCountedThreadSafe<TestVideoEncoderCallback> {
 public:
  TestVideoEncoderCallback() {}

  void SetExpectedResult(uint32 expected_frame_id,
                         uint32 expected_last_referenced_frame_id,
                         uint32 expected_rtp_timestamp,
                         const base::TimeTicks& expected_reference_time) {
    expected_frame_id_ = expected_frame_id;
    expected_last_referenced_frame_id_ = expected_last_referenced_frame_id;
    expected_rtp_timestamp_ = expected_rtp_timestamp;
    expected_reference_time_ = expected_reference_time;
  }

  void DeliverEncodedVideoFrame(
      scoped_ptr<EncodedFrame> encoded_frame) {
    if (expected_frame_id_ == expected_last_referenced_frame_id_) {
      EXPECT_EQ(EncodedFrame::KEY, encoded_frame->dependency);
    } else {
      EXPECT_EQ(EncodedFrame::DEPENDENT,
                encoded_frame->dependency);
    }
    EXPECT_EQ(expected_frame_id_, encoded_frame->frame_id);
    EXPECT_EQ(expected_last_referenced_frame_id_,
              encoded_frame->referenced_frame_id);
    EXPECT_EQ(expected_rtp_timestamp_, encoded_frame->rtp_timestamp);
    EXPECT_EQ(expected_reference_time_, encoded_frame->reference_time);
  }

 protected:
  virtual ~TestVideoEncoderCallback() {}

 private:
  friend class base::RefCountedThreadSafe<TestVideoEncoderCallback>;

  uint32 expected_frame_id_;
  uint32 expected_last_referenced_frame_id_;
  uint32 expected_rtp_timestamp_;
  base::TimeTicks expected_reference_time_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoEncoderCallback);
};
}  // namespace

class ExternalVideoEncoderTest : public ::testing::Test {
 protected:
  ExternalVideoEncoderTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_)),
        vea_factory_(task_runner_),
        init_status_(STATUS_VIDEO_UNINITIALIZED),
        test_video_encoder_callback_(new TestVideoEncoderCallback()) {
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());

    video_config_.ssrc = 1;
    video_config_.receiver_ssrc = 2;
    video_config_.rtp_payload_type = 127;
    video_config_.use_external_encoder = true;
    video_config_.max_bitrate = 5000000;
    video_config_.min_bitrate = 1000000;
    video_config_.start_bitrate = 2000000;
    video_config_.max_qp = 56;
    video_config_.min_qp = 0;
    video_config_.max_frame_rate = 30;
    video_config_.max_number_of_video_buffers_used = 3;
    video_config_.codec = CODEC_VIDEO_VP8;
    const gfx::Size size(320, 240);
    video_frame_ = media::VideoFrame::CreateFrame(
        VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(video_frame_.get(), 123);

    video_encoder_.reset(new ExternalVideoEncoder(
        cast_environment_,
        video_config_,
        size,
        base::Bind(&ExternalVideoEncoderTest::SaveInitializationStatus,
                   base::Unretained(this)),
        base::Bind(
            &FakeVideoEncodeAcceleratorFactory::CreateVideoEncodeAccelerator,
            base::Unretained(&vea_factory_)),
        base::Bind(&FakeVideoEncodeAcceleratorFactory::CreateSharedMemory,
                   base::Unretained(&vea_factory_))));
  }

  ~ExternalVideoEncoderTest() override {}

  void AdvanceClockAndVideoFrameTimestamp() {
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(33));
    video_frame_->set_timestamp(
        video_frame_->timestamp() + base::TimeDelta::FromMilliseconds(33));
  }

  void SaveInitializationStatus(CastInitializationStatus result) {
    EXPECT_EQ(STATUS_VIDEO_UNINITIALIZED, init_status_);
    init_status_ = result;
  }

  base::SimpleTestTickClock* const testing_clock_;  // Owned by CastEnvironment.
  const scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  const scoped_refptr<CastEnvironment> cast_environment_;
  FakeVideoEncodeAcceleratorFactory vea_factory_;
  CastInitializationStatus init_status_;
  scoped_refptr<TestVideoEncoderCallback> test_video_encoder_callback_;
  VideoSenderConfig video_config_;
  scoped_ptr<VideoEncoder> video_encoder_;
  scoped_refptr<media::VideoFrame> video_frame_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVideoEncoderTest);
};

TEST_F(ExternalVideoEncoderTest, EncodePattern30fpsRunningOutOfAck) {
  vea_factory_.SetAutoRespond(true);
  task_runner_->RunTasks();  // Run the initializer on the correct thread.
  EXPECT_EQ(STATUS_VIDEO_INITIALIZED, init_status_);

  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  test_video_encoder_callback_->SetExpectedResult(
      0, 0, TimeDeltaToRtpDelta(video_frame_->timestamp(), kVideoFrequency),
      testing_clock_->NowTicks());
  video_encoder_->SetBitRate(2000);
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
      video_frame_, testing_clock_->NowTicks(), frame_encoded_callback));
  task_runner_->RunTasks();

  for (int i = 0; i < 6; ++i) {
    AdvanceClockAndVideoFrameTimestamp();
    test_video_encoder_callback_->SetExpectedResult(
        i + 1,
        i,
        TimeDeltaToRtpDelta(video_frame_->timestamp(), kVideoFrequency),
        testing_clock_->NowTicks());
    EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
        video_frame_, testing_clock_->NowTicks(), frame_encoded_callback));
    task_runner_->RunTasks();
  }

  ASSERT_EQ(1u, vea_factory_.last_response_vea()->stored_bitrates().size());
  EXPECT_EQ(2000u, vea_factory_.last_response_vea()->stored_bitrates()[0]);

  // We need to run the task to cleanup the GPU instance.
  video_encoder_.reset(NULL);
  task_runner_->RunTasks();

  EXPECT_EQ(1, vea_factory_.vea_response_count());
  EXPECT_EQ(3, vea_factory_.shm_response_count());
}

TEST_F(ExternalVideoEncoderTest, StreamHeader) {
  vea_factory_.SetAutoRespond(true);
  task_runner_->RunTasks();  // Run the initializer on the correct thread.
  EXPECT_EQ(STATUS_VIDEO_INITIALIZED, init_status_);

  VideoEncoder::FrameEncodedCallback frame_encoded_callback =
      base::Bind(&TestVideoEncoderCallback::DeliverEncodedVideoFrame,
                 test_video_encoder_callback_.get());

  // Force the FakeVideoEncodeAccelerator to return a dummy non-key frame first.
  vea_factory_.last_response_vea()->SendDummyFrameForTesting(false);

  // Verify the first returned bitstream buffer is still a key frame.
  test_video_encoder_callback_->SetExpectedResult(
      0, 0, TimeDeltaToRtpDelta(video_frame_->timestamp(), kVideoFrequency),
      testing_clock_->NowTicks());
  EXPECT_TRUE(video_encoder_->EncodeVideoFrame(
      video_frame_, testing_clock_->NowTicks(), frame_encoded_callback));
  task_runner_->RunTasks();

  // We need to run the task to cleanup the GPU instance.
  video_encoder_.reset(NULL);
  task_runner_->RunTasks();

  EXPECT_EQ(1, vea_factory_.vea_response_count());
  EXPECT_EQ(3, vea_factory_.shm_response_count());
}

// Verify that everything goes well even if ExternalVideoEncoder is destroyed
// before it has a chance to receive the VEA creation callback.
TEST_F(ExternalVideoEncoderTest, DestroyBeforeVEACreatedCallback) {
  video_encoder_.reset();
  EXPECT_EQ(0, vea_factory_.vea_response_count());
  vea_factory_.SetAutoRespond(true);
  task_runner_->RunTasks();
  EXPECT_EQ(1, vea_factory_.vea_response_count());
  EXPECT_EQ(STATUS_VIDEO_UNINITIALIZED, init_status_);
}

}  // namespace cast
}  // namespace media

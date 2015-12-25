// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "media/base/gmock_callback_support.h"
#include "media/renderers/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_decode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;

namespace content {

// TODO(wuchengli): add MockSharedMemroy so more functions can be tested.
class RTCVideoDecoderTest : public ::testing::Test,
                            webrtc::DecodedImageCallback {
 public:
  RTCVideoDecoderTest()
      : mock_gpu_factories_(
            new media::MockGpuVideoAcceleratorFactories(nullptr)),
        vda_thread_("vda_thread"),
        idle_waiter_(false, false) {
    memset(&codec_, 0, sizeof(codec_));
  }

  void SetUp() override {
    ASSERT_TRUE(vda_thread_.Start());
    vda_task_runner_ = vda_thread_.task_runner();
    mock_vda_ = new media::MockVideoDecodeAccelerator;

    media::VideoDecodeAccelerator::SupportedProfile supported_profile;
    supported_profile.min_resolution.SetSize(16, 16);
    supported_profile.max_resolution.SetSize(1920, 1088);
    supported_profile.profile = media::H264PROFILE_MAIN;
    capabilities_.supported_profiles.push_back(supported_profile);
    supported_profile.profile = media::VP8PROFILE_ANY;
    capabilities_.supported_profiles.push_back(supported_profile);

    EXPECT_CALL(*mock_gpu_factories_.get(), GetTaskRunner())
        .WillRepeatedly(Return(vda_task_runner_));
    EXPECT_CALL(*mock_gpu_factories_.get(),
                GetVideoDecodeAcceleratorCapabilities())
        .WillRepeatedly(Return(capabilities_));
    EXPECT_CALL(*mock_gpu_factories_.get(), DoCreateVideoDecodeAccelerator())
        .WillRepeatedly(Return(mock_vda_));
    EXPECT_CALL(*mock_vda_, Initialize(_, _))
        .Times(1)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_vda_, Destroy()).Times(1);
  }

  void TearDown() override {
    DVLOG(2) << "TearDown";
    EXPECT_TRUE(vda_thread_.IsRunning());
    RunUntilIdle();  // Wait until all callbascks complete.
    vda_task_runner_->DeleteSoon(FROM_HERE, rtc_decoder_.release());
    // Make sure the decoder is released before stopping the thread.
    RunUntilIdle();
    vda_thread_.Stop();
  }

  int32_t Decoded(webrtc::VideoFrame& decoded_image) override {
    DVLOG(2) << "Decoded";
    EXPECT_EQ(vda_task_runner_, base::ThreadTaskRunnerHandle::Get());
    return WEBRTC_VIDEO_CODEC_OK;
  }

  void CreateDecoder(webrtc::VideoCodecType codec_type) {
    DVLOG(2) << "CreateDecoder";
    codec_.codecType = codec_type;
    rtc_decoder_ =
        RTCVideoDecoder::Create(codec_type, mock_gpu_factories_.get());
  }

  void Initialize() {
    DVLOG(2) << "Initialize";
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->InitDecode(&codec_, 1));
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_decoder_->RegisterDecodeCompleteCallback(this));
  }

  void NotifyResetDone() {
    DVLOG(2) << "NotifyResetDone";
    vda_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::NotifyResetDone,
                   base::Unretained(rtc_decoder_.get())));
  }

  void RunUntilIdle() {
    DVLOG(2) << "RunUntilIdle";
    vda_task_runner_->PostTask(FROM_HERE,
                               base::Bind(&base::WaitableEvent::Signal,
                                          base::Unretained(&idle_waiter_)));
    idle_waiter_.Wait();
  }

 protected:
  scoped_ptr<media::MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  media::MockVideoDecodeAccelerator* mock_vda_;
  scoped_ptr<RTCVideoDecoder> rtc_decoder_;
  webrtc::VideoCodec codec_;
  base::Thread vda_thread_;
  media::VideoDecodeAccelerator::Capabilities capabilities_;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> vda_task_runner_;

  base::Lock lock_;
  base::WaitableEvent idle_waiter_;
};

TEST_F(RTCVideoDecoderTest, CreateReturnsNullOnUnsupportedCodec) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  scoped_ptr<RTCVideoDecoder> null_rtc_decoder(RTCVideoDecoder::Create(
      webrtc::kVideoCodecI420, mock_gpu_factories_.get()));
  EXPECT_EQ(NULL, null_rtc_decoder.get());
}

TEST_F(RTCVideoDecoderTest, CreateAndInitSucceedsForH264Codec) {
  CreateDecoder(webrtc::kVideoCodecH264);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->InitDecode(&codec_, 1));
}

TEST_F(RTCVideoDecoderTest, InitDecodeReturnsErrorOnFeedbackMode) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  codec_.codecSpecific.VP8.feedbackModeOn = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR, rtc_decoder_->InitDecode(&codec_, 1));
}

TEST_F(RTCVideoDecoderTest, DecodeReturnsErrorWithoutInitDecode) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  webrtc::EncodedImage input_image;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_UNINITIALIZED,
            rtc_decoder_->Decode(input_image, false, NULL, NULL, 0));
}

TEST_F(RTCVideoDecoderTest, DecodeReturnsErrorOnIncompleteFrame) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  Initialize();
  webrtc::EncodedImage input_image;
  input_image._completeFrame = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            rtc_decoder_->Decode(input_image, false, NULL, NULL, 0));
}

TEST_F(RTCVideoDecoderTest, DecodeReturnsErrorOnMissingFrames) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  Initialize();
  webrtc::EncodedImage input_image;
  input_image._completeFrame = true;
  bool missingFrames = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            rtc_decoder_->Decode(input_image, missingFrames, NULL, NULL, 0));
}

TEST_F(RTCVideoDecoderTest, ResetReturnsOk) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  Initialize();
  EXPECT_CALL(*mock_vda_, Reset())
      .WillOnce(Invoke(this, &RTCVideoDecoderTest::NotifyResetDone));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Reset());
}

TEST_F(RTCVideoDecoderTest, ReleaseReturnsOk) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  Initialize();
  EXPECT_CALL(*mock_vda_, Reset())
      .WillOnce(Invoke(this, &RTCVideoDecoderTest::NotifyResetDone));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Release());
}

TEST_F(RTCVideoDecoderTest, InitDecodeAfterRelease) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  EXPECT_CALL(*mock_vda_, Reset())
      .WillRepeatedly(Invoke(this, &RTCVideoDecoderTest::NotifyResetDone));
  Initialize();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Release());
  Initialize();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_decoder_->Release());
}

TEST_F(RTCVideoDecoderTest, IsBufferAfterReset) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  EXPECT_TRUE(rtc_decoder_->IsBufferAfterReset(0, RTCVideoDecoder::ID_INVALID));
  EXPECT_TRUE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_LAST,
                                               RTCVideoDecoder::ID_INVALID));
  EXPECT_FALSE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_HALF - 2,
                                                RTCVideoDecoder::ID_HALF + 2));
  EXPECT_TRUE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_HALF + 2,
                                               RTCVideoDecoder::ID_HALF - 2));

  EXPECT_FALSE(rtc_decoder_->IsBufferAfterReset(0, 0));
  EXPECT_TRUE(rtc_decoder_->IsBufferAfterReset(0, RTCVideoDecoder::ID_LAST));
  EXPECT_FALSE(
      rtc_decoder_->IsBufferAfterReset(0, RTCVideoDecoder::ID_HALF - 2));
  EXPECT_TRUE(
      rtc_decoder_->IsBufferAfterReset(0, RTCVideoDecoder::ID_HALF + 2));

  EXPECT_FALSE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_LAST, 0));
  EXPECT_FALSE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_LAST,
                                                RTCVideoDecoder::ID_HALF - 2));
  EXPECT_TRUE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_LAST,
                                               RTCVideoDecoder::ID_HALF + 2));
  EXPECT_FALSE(rtc_decoder_->IsBufferAfterReset(RTCVideoDecoder::ID_LAST,
                                                RTCVideoDecoder::ID_LAST));
}

TEST_F(RTCVideoDecoderTest, IsFirstBufferAfterReset) {
  CreateDecoder(webrtc::kVideoCodecVP8);
  EXPECT_TRUE(
      rtc_decoder_->IsFirstBufferAfterReset(0, RTCVideoDecoder::ID_INVALID));
  EXPECT_FALSE(
      rtc_decoder_->IsFirstBufferAfterReset(1, RTCVideoDecoder::ID_INVALID));
  EXPECT_FALSE(rtc_decoder_->IsFirstBufferAfterReset(0, 0));
  EXPECT_TRUE(rtc_decoder_->IsFirstBufferAfterReset(1, 0));
  EXPECT_FALSE(rtc_decoder_->IsFirstBufferAfterReset(2, 0));

  EXPECT_FALSE(rtc_decoder_->IsFirstBufferAfterReset(RTCVideoDecoder::ID_HALF,
                                                     RTCVideoDecoder::ID_HALF));
  EXPECT_TRUE(rtc_decoder_->IsFirstBufferAfterReset(
      RTCVideoDecoder::ID_HALF + 1, RTCVideoDecoder::ID_HALF));
  EXPECT_FALSE(rtc_decoder_->IsFirstBufferAfterReset(
      RTCVideoDecoder::ID_HALF + 2, RTCVideoDecoder::ID_HALF));

  EXPECT_FALSE(rtc_decoder_->IsFirstBufferAfterReset(RTCVideoDecoder::ID_LAST,
                                                     RTCVideoDecoder::ID_LAST));
  EXPECT_TRUE(
      rtc_decoder_->IsFirstBufferAfterReset(0, RTCVideoDecoder::ID_LAST));
  EXPECT_FALSE(
      rtc_decoder_->IsFirstBufferAfterReset(1, RTCVideoDecoder::ID_LAST));
}

}  // content

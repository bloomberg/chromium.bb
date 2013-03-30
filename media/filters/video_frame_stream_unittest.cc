// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/video_frame_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::AtMost;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

static const VideoFrame::Format kVideoFormat = VideoFrame::YV12;
static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(320, 240);

class VideoFrameStreamTest : public testing::TestWithParam<bool> {
 public:
  VideoFrameStreamTest()
      : video_frame_stream_(new VideoFrameStream(
            message_loop_.message_loop_proxy(),
            base::Bind(&VideoFrameStreamTest::SetDecryptorReadyCallback,
                       base::Unretained(this)))),
        video_config_(kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN, kVideoFormat,
                      kCodedSize, kVisibleRect, kNaturalSize, NULL, 0,
                      GetParam()),
        demuxer_stream_(new StrictMock<MockDemuxerStream>()),
        decryptor_(new NiceMock<MockDecryptor>()),
        decoder_(new StrictMock<MockVideoDecoder>()),
        is_initialized_(false) {
    decoders_.push_back(decoder_);

    EXPECT_CALL(*demuxer_stream_, type())
        .WillRepeatedly(Return(DemuxerStream::VIDEO));
    EXPECT_CALL(*demuxer_stream_, video_decoder_config())
        .WillRepeatedly(ReturnRef(video_config_));
    EXPECT_CALL(*demuxer_stream_, Read(_))
        .WillRepeatedly(RunCallback<0>(DemuxerStream::kOk,
                                       scoped_refptr<DecoderBuffer>()));

    EXPECT_CALL(*this, SetDecryptorReadyCallback(_))
        .WillRepeatedly(RunCallback<0>(decryptor_.get()));

    // Decryptor can only decrypt (not decrypt-and-decode) so that
    // DecryptingDemuxerStream will be used.
    EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
        .WillRepeatedly(RunCallback<1>(false));
    EXPECT_CALL(*decryptor_, Decrypt(_, _, _))
        .WillRepeatedly(RunCallback<2>(Decryptor::kSuccess,
                                       scoped_refptr<DecoderBuffer>()));
  }

  ~VideoFrameStreamTest() {
    if (is_initialized_)
      Stop();

    EXPECT_FALSE(is_initialized_);
    EXPECT_TRUE(decoder_init_cb_.is_null());
    EXPECT_TRUE(decoder_read_cb_.is_null());
    EXPECT_TRUE(decoder_reset_cb_.is_null());
    EXPECT_TRUE(decoder_stop_cb_.is_null());
  }

  MOCK_METHOD1(OnStatistics, void(const PipelineStatistics&));
  MOCK_METHOD1(SetDecryptorReadyCallback, void(const media::DecryptorReadyCB&));
  MOCK_METHOD2(OnInitialized, void(bool, bool));
  MOCK_METHOD2(OnFrameRead, void(VideoDecoder::Status,
                                 const scoped_refptr<VideoFrame>&));
  MOCK_METHOD0(OnReset, void());
  MOCK_METHOD0(OnStopped, void());

  void EnterPendingInitializationState() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(SaveArg<1>(&decoder_init_cb_));
    video_frame_stream_->Initialize(
        demuxer_stream_,
        decoders_,
        base::Bind(&VideoFrameStreamTest::OnStatistics, base::Unretained(this)),
        base::Bind(&VideoFrameStreamTest::OnInitialized,
                   base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void SatisfyPendingInitialization(bool success) {
    EXPECT_CALL(*this, OnInitialized(success, false))
        .WillOnce(SaveArg<0>(&is_initialized_));
    base::ResetAndReturn(&decoder_init_cb_).Run(
        success ? PIPELINE_OK : DECODER_ERROR_NOT_SUPPORTED);
    message_loop_.RunUntilIdle();
  }

  void EnterPendingReadFrameState() {
    EXPECT_CALL(*decoder_, Read(_))
        .WillOnce(SaveArg<0>(&decoder_read_cb_));
    EXPECT_CALL(*this, OnFrameRead(VideoDecoder::kOk, _));
    video_frame_stream_->ReadFrame(base::Bind(
        &VideoFrameStreamTest::OnFrameRead, base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void SatisfyPendingReadFrame() {
    base::ResetAndReturn(&decoder_read_cb_).Run(VideoDecoder::kOk,
                                                scoped_refptr<VideoFrame>());
    message_loop_.RunUntilIdle();
  }

  void EnterPendingResetState() {
    EXPECT_CALL(*decoder_, Reset(_))
        .WillOnce(SaveArg<0>(&decoder_reset_cb_));
    EXPECT_CALL(*this, OnReset());
    video_frame_stream_->Reset(base::Bind(&VideoFrameStreamTest::OnReset,
                                          base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void SatisfyPendingReset() {
    base::ResetAndReturn(&decoder_reset_cb_).Run();
    message_loop_.RunUntilIdle();
  }

  void EnterPendingStopState() {
    // If initialization failed, we won't call VideoDecoder::Stop() during
    // the stopping process.
    EXPECT_CALL(*decoder_, Stop(_))
        .Times(AtMost(1))
        .WillRepeatedly(SaveArg<0>(&decoder_stop_cb_));
    EXPECT_CALL(*this, OnStopped())
        .WillOnce(Assign(&is_initialized_, false));
    video_frame_stream_->Stop(base::Bind(&VideoFrameStreamTest::OnStopped,
                                         base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void SatisfyPendingStop() {
    // If decoder is not initialized, |decoder_stop_cb_| can be null. In that
    // case, we don't actually need to satisfy the stop callback.
    if (decoder_stop_cb_.is_null())
      return;
    base::ResetAndReturn(&decoder_stop_cb_).Run();
    message_loop_.RunUntilIdle();
  }

  void Initialize() {
    EnterPendingInitializationState();
    SatisfyPendingInitialization(true);
  }

  void InitializeAndFail() {
    EnterPendingInitializationState();
    SatisfyPendingInitialization(false);
  }

  void ReadFrame() {
    EnterPendingReadFrameState();
    SatisfyPendingReadFrame();
  }

  void Reset() {
    EnterPendingResetState();
    SatisfyPendingReset();
  }

  void Stop() {
    EnterPendingStopState();
    SatisfyPendingStop();
  }

 private:
  MessageLoop message_loop_;

  scoped_refptr<VideoFrameStream> video_frame_stream_;
  VideoDecoderConfig video_config_;
  scoped_refptr<StrictMock<MockDemuxerStream> > demuxer_stream_;
  // Use NiceMock since we don't care about most of calls on the decryptor, e.g.
  // RegisterNewKeyCB().
  scoped_ptr<NiceMock<MockDecryptor> > decryptor_;
  scoped_refptr<StrictMock<MockVideoDecoder> > decoder_;
  VideoFrameStream::VideoDecoderList decoders_;

  // Callbacks to simulate pending decoder operations.
  PipelineStatusCB decoder_init_cb_;
  VideoDecoder::ReadCB decoder_read_cb_;
  base::Closure decoder_reset_cb_;
  base::Closure decoder_stop_cb_;

  bool is_initialized_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameStreamTest);
};

INSTANTIATE_TEST_CASE_P(Clear, VideoFrameStreamTest, testing::Values(false));
INSTANTIATE_TEST_CASE_P(Encrypted, VideoFrameStreamTest, testing::Values(true));

TEST_P(VideoFrameStreamTest, Initialization) {
  Initialize();
}

TEST_P(VideoFrameStreamTest, Initialization_Failed) {
  InitializeAndFail();
}

TEST_P(VideoFrameStreamTest, ReadFrame) {
  Initialize();
  ReadFrame();
}

TEST_P(VideoFrameStreamTest, ReadFrame_Multiple) {
  Initialize();
  for (int i = 0; i < 10; ++i)
    ReadFrame();
}

TEST_P(VideoFrameStreamTest, ReadFrame_AfterReset) {
  Initialize();
  Reset();
  ReadFrame();
}

TEST_P(VideoFrameStreamTest, Reset_AfterInitialization) {
  Initialize();
  Reset();
}

TEST_P(VideoFrameStreamTest, Reset_DuringReadFrame) {
  Initialize();
  EnterPendingReadFrameState();
  EnterPendingResetState();
  SatisfyPendingReadFrame();
  SatisfyPendingReset();
}

TEST_P(VideoFrameStreamTest, Reset_AfterReadFrame) {
  Initialize();
  ReadFrame();
  Reset();
}

TEST_P(VideoFrameStreamTest, Stop_BeforeInitialization) {
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringInitialization) {
  EnterPendingInitializationState();
  EnterPendingStopState();
  SatisfyPendingInitialization(true);
  SatisfyPendingStop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringFailedInitialization) {
  EnterPendingInitializationState();
  EnterPendingStopState();
  SatisfyPendingInitialization(false);
  SatisfyPendingStop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterInitialization) {
  Initialize();
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterFailedInitialization) {
  InitializeAndFail();
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringReadFrame) {
  Initialize();
  EnterPendingReadFrameState();
  EnterPendingStopState();
  SatisfyPendingReadFrame();
  SatisfyPendingStop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterReadFrame) {
  Initialize();
  ReadFrame();
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringReset) {
  Initialize();
  EnterPendingResetState();
  EnterPendingStopState();
  SatisfyPendingReset();
  SatisfyPendingStop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterReset) {
  Initialize();
  Reset();
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringReadFrame_DuringReset) {
  Initialize();
  EnterPendingReadFrameState();
  EnterPendingResetState();
  EnterPendingStopState();
  SatisfyPendingReadFrame();
  SatisfyPendingReset();
  SatisfyPendingStop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterReadFrame_DuringReset) {
  Initialize();
  EnterPendingReadFrameState();
  EnterPendingResetState();
  SatisfyPendingReadFrame();
  EnterPendingStopState();
  SatisfyPendingReset();
  SatisfyPendingStop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterReadFrame_AfterReset) {
  Initialize();
  ReadFrame();
  Reset();
  Stop();
}

}  // namespace media

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/filters/fake_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kDecodingDelay = 9;
static const int kTotalBuffers = 12;
static const int kDurationMs = 30;

class FakeVideoDecoderTest : public testing::Test {
 public:
  FakeVideoDecoderTest()
      : decoder_(new FakeVideoDecoder(kDecodingDelay)),
        num_input_buffers_(0),
        num_decoded_frames_(0),
        decode_status_(VideoDecoder::kNotEnoughData),
        is_decode_pending_(false),
        is_reset_pending_(false),
        is_stop_pending_(false) {}

  virtual ~FakeVideoDecoderTest() {
    StopAndExpect(OK);
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    decoder_->Initialize(config, NewExpectedStatusCB(PIPELINE_OK));
    message_loop_.RunUntilIdle();
    current_config_ = config;
  }

  void Initialize() {
    InitializeWithConfig(TestVideoConfig::Normal());
  }

  void EnterPendingInitState() {
    decoder_->HoldNextInit();
    Initialize();
  }

  void SatisfyInit() {
    decoder_->SatisfyInit();
    message_loop_.RunUntilIdle();
  }

  // Callback for VideoDecoder::Read().
  void FrameReady(VideoDecoder::Status status,
                  const scoped_refptr<VideoFrame>& frame) {
    DCHECK(is_decode_pending_);
    ASSERT_TRUE(status == VideoDecoder::kOk ||
                status == VideoDecoder::kNotEnoughData);
    is_decode_pending_ = false;
    decode_status_ = status;
    frame_decoded_ = frame;

    if (frame && !frame->end_of_stream())
      num_decoded_frames_++;
  }

  enum CallbackResult {
    PENDING,
    OK,
    NOT_ENOUGH_DATA,
    ABROTED,
    EOS
  };

  void ExpectReadResult(CallbackResult result) {
    switch (result) {
      case PENDING:
        EXPECT_TRUE(is_decode_pending_);
        ASSERT_FALSE(frame_decoded_);
        break;
      case OK:
        EXPECT_FALSE(is_decode_pending_);
        ASSERT_EQ(VideoDecoder::kOk, decode_status_);
        ASSERT_TRUE(frame_decoded_);
        EXPECT_FALSE(frame_decoded_->end_of_stream());
        break;
      case NOT_ENOUGH_DATA:
        EXPECT_FALSE(is_decode_pending_);
        ASSERT_EQ(VideoDecoder::kNotEnoughData, decode_status_);
        ASSERT_FALSE(frame_decoded_);
        break;
      case ABROTED:
        EXPECT_FALSE(is_decode_pending_);
        ASSERT_EQ(VideoDecoder::kOk, decode_status_);
        EXPECT_FALSE(frame_decoded_);
        break;
      case EOS:
        EXPECT_FALSE(is_decode_pending_);
        ASSERT_EQ(VideoDecoder::kOk, decode_status_);
        ASSERT_TRUE(frame_decoded_);
        EXPECT_TRUE(frame_decoded_->end_of_stream());
        break;
    }
  }

  void Decode() {
    scoped_refptr<DecoderBuffer> buffer;

    if (num_input_buffers_ < kTotalBuffers) {
      buffer = CreateFakeVideoBufferForTest(
          current_config_,
          base::TimeDelta::FromMilliseconds(kDurationMs * num_input_buffers_),
          base::TimeDelta::FromMilliseconds(kDurationMs));
      num_input_buffers_++;
    } else {
      buffer = DecoderBuffer::CreateEOSBuffer();
    }

    decode_status_ = VideoDecoder::kDecodeError;
    frame_decoded_ = NULL;
    is_decode_pending_ = true;

    decoder_->Decode(
        buffer,
        base::Bind(&FakeVideoDecoderTest::FrameReady, base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void ReadOneFrame() {
    do {
      Decode();
    } while (decode_status_ == VideoDecoder::kNotEnoughData &&
             !is_decode_pending_);
  }

  void ReadUntilEOS() {
    do {
      ReadOneFrame();
    } while (frame_decoded_ && !frame_decoded_->end_of_stream());
  }

  void EnterPendingReadState() {
    // Pass the initial NOT_ENOUGH_DATA stage.
    ReadOneFrame();
    decoder_->HoldNextRead();
    ReadOneFrame();
    ExpectReadResult(PENDING);
  }

  void SatisfyReadAndExpect(CallbackResult result) {
    decoder_->SatisfyRead();
    message_loop_.RunUntilIdle();
    ExpectReadResult(result);
  }

  void SatisfyRead() {
    SatisfyReadAndExpect(OK);
  }

  // Callback for VideoDecoder::Reset().
  void OnDecoderReset() {
    DCHECK(is_reset_pending_);
    is_reset_pending_ = false;
  }

  void ExpectResetResult(CallbackResult result) {
    switch (result) {
      case PENDING:
        EXPECT_TRUE(is_reset_pending_);
        break;
      case OK:
        EXPECT_FALSE(is_reset_pending_);
        break;
      default:
        NOTREACHED();
    }
  }

  void ResetAndExpect(CallbackResult result) {
    is_reset_pending_ = true;
    decoder_->Reset(base::Bind(&FakeVideoDecoderTest::OnDecoderReset,
                               base::Unretained(this)));
    message_loop_.RunUntilIdle();
    ExpectResetResult(result);
  }

  void EnterPendingResetState() {
    decoder_->HoldNextReset();
    ResetAndExpect(PENDING);
  }

  void SatisfyReset() {
    decoder_->SatisfyReset();
    message_loop_.RunUntilIdle();
    ExpectResetResult(OK);
  }

  // Callback for VideoDecoder::Stop().
  void OnDecoderStopped() {
    DCHECK(is_stop_pending_);
    is_stop_pending_ = false;
  }

  void ExpectStopResult(CallbackResult result) {
    switch (result) {
      case PENDING:
        EXPECT_TRUE(is_stop_pending_);
        break;
      case OK:
        EXPECT_FALSE(is_stop_pending_);
        break;
      default:
        NOTREACHED();
    }
  }

  void StopAndExpect(CallbackResult result) {
    is_stop_pending_ = true;
    decoder_->Stop(base::Bind(&FakeVideoDecoderTest::OnDecoderStopped,
                              base::Unretained(this)));
    message_loop_.RunUntilIdle();
    ExpectStopResult(result);
  }

  void EnterPendingStopState() {
    decoder_->HoldNextStop();
    StopAndExpect(PENDING);
  }

  void SatisfyStop() {
    decoder_->SatisfyStop();
    message_loop_.RunUntilIdle();
    ExpectStopResult(OK);
  }

  base::MessageLoop message_loop_;
  VideoDecoderConfig current_config_;

  scoped_ptr<FakeVideoDecoder> decoder_;

  int num_input_buffers_;
  int num_decoded_frames_;

  // Callback result/status.
  VideoDecoder::Status decode_status_;
  scoped_refptr<VideoFrame> frame_decoded_;
  bool is_decode_pending_;
  bool is_reset_pending_;
  bool is_stop_pending_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeVideoDecoderTest);
};

TEST_F(FakeVideoDecoderTest, Initialize) {
  Initialize();
}

TEST_F(FakeVideoDecoderTest, Read_AllFrames) {
  Initialize();
  ReadUntilEOS();
  EXPECT_EQ(kTotalBuffers, num_decoded_frames_);
}

TEST_F(FakeVideoDecoderTest, Read_DecodingDelay) {
  Initialize();

  while (num_input_buffers_ < kTotalBuffers) {
    ReadOneFrame();
    EXPECT_EQ(num_input_buffers_, num_decoded_frames_ + kDecodingDelay);
  }
}

TEST_F(FakeVideoDecoderTest, Read_ZeroDelay) {
  decoder_.reset(new FakeVideoDecoder(0));
  Initialize();

  while (num_input_buffers_ < kTotalBuffers) {
    ReadOneFrame();
    EXPECT_EQ(num_input_buffers_, num_decoded_frames_);
  }
}

TEST_F(FakeVideoDecoderTest, Read_Pending_NotEnoughData) {
  Initialize();
  decoder_->HoldNextRead();
  ReadOneFrame();
  ExpectReadResult(PENDING);
  SatisfyReadAndExpect(NOT_ENOUGH_DATA);
}

TEST_F(FakeVideoDecoderTest, Read_Pending_OK) {
  Initialize();
  ReadOneFrame();
  EnterPendingReadState();
  SatisfyReadAndExpect(OK);
}

TEST_F(FakeVideoDecoderTest, Reinitialize) {
  Initialize();
  ReadOneFrame();
  InitializeWithConfig(TestVideoConfig::Large());
  ReadOneFrame();
}

// Reinitializing the decoder during the middle of the decoding process can
// cause dropped frames.
TEST_F(FakeVideoDecoderTest, Reinitialize_FrameDropped) {
  Initialize();
  ReadOneFrame();
  Initialize();
  ReadUntilEOS();
  EXPECT_LT(num_decoded_frames_, kTotalBuffers);
}

TEST_F(FakeVideoDecoderTest, Reset) {
  Initialize();
  ReadOneFrame();
  ResetAndExpect(OK);
}

TEST_F(FakeVideoDecoderTest, Reset_DuringPendingRead) {
  Initialize();
  EnterPendingReadState();
  ResetAndExpect(PENDING);
  SatisfyRead();
}

TEST_F(FakeVideoDecoderTest, Reset_Pending) {
  Initialize();
  EnterPendingResetState();
  SatisfyReset();
}

TEST_F(FakeVideoDecoderTest, Reset_PendingDuringPendingRead) {
  Initialize();
  EnterPendingReadState();
  EnterPendingResetState();
  SatisfyRead();
  SatisfyReset();
}

TEST_F(FakeVideoDecoderTest, Stop) {
  Initialize();
  ReadOneFrame();
  ExpectReadResult(OK);
  StopAndExpect(OK);
}

TEST_F(FakeVideoDecoderTest, Stop_DuringPendingInitialization) {
  EnterPendingInitState();
  EnterPendingStopState();
  SatisfyInit();
  SatisfyStop();
}

TEST_F(FakeVideoDecoderTest, Stop_DuringPendingRead) {
  Initialize();
  EnterPendingReadState();
  StopAndExpect(PENDING);
  SatisfyRead();
  ExpectStopResult(OK);
}

TEST_F(FakeVideoDecoderTest, Stop_DuringPendingReset) {
  Initialize();
  EnterPendingResetState();
  StopAndExpect(PENDING);
  SatisfyReset();
  ExpectStopResult(OK);
}

TEST_F(FakeVideoDecoderTest, Stop_DuringPendingReadAndPendingReset) {
  Initialize();
  EnterPendingReadState();
  EnterPendingResetState();
  StopAndExpect(PENDING);
  SatisfyRead();
  SatisfyReset();
  ExpectStopResult(OK);
}

TEST_F(FakeVideoDecoderTest, Stop_Pending) {
  Initialize();
  decoder_->HoldNextStop();
  StopAndExpect(PENDING);
  decoder_->SatisfyStop();
  message_loop_.RunUntilIdle();
  ExpectStopResult(OK);
}

TEST_F(FakeVideoDecoderTest, Stop_PendingDuringPendingRead) {
  Initialize();
  EnterPendingReadState();
  EnterPendingStopState();
  SatisfyRead();
  SatisfyStop();
}

TEST_F(FakeVideoDecoderTest, Stop_PendingDuringPendingReset) {
  Initialize();
  EnterPendingResetState();
  EnterPendingStopState();
  SatisfyReset();
  SatisfyStop();
}

TEST_F(FakeVideoDecoderTest, Stop_PendingDuringPendingReadAndPendingReset) {
  Initialize();
  EnterPendingReadState();
  EnterPendingResetState();
  EnterPendingStopState();
  SatisfyRead();
  SatisfyReset();
  SatisfyStop();
}

}  // namespace media

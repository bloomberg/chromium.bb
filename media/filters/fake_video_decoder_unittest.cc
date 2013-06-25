// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/filters/fake_demuxer_stream.h"
#include "media/filters/fake_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kDecodingDelay = 9;
static const int kNumConfigs = 3;
static const int kNumBuffersInOneConfig = 9;
static const int kNumInputBuffers = kNumConfigs * kNumBuffersInOneConfig;

class FakeVideoDecoderTest : public testing::Test {
 public:
  FakeVideoDecoderTest()
      : decoder_(new FakeVideoDecoder(kDecodingDelay)),
        demuxer_stream_(
            new FakeDemuxerStream(kNumConfigs, kNumBuffersInOneConfig, false)),
        num_decoded_frames_(0),
        is_read_pending_(false),
        is_reset_pending_(false),
        is_stop_pending_(false) {}

  virtual ~FakeVideoDecoderTest() {
    StopAndExpect(OK);
  }

  void Initialize() {
    decoder_->Initialize(demuxer_stream_.get(),
                         NewExpectedStatusCB(PIPELINE_OK),
                         base::Bind(&MockStatisticsCB::OnStatistics,
                                    base::Unretained(&statistics_cb_)));
    message_loop_.RunUntilIdle();
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
    DCHECK(is_read_pending_);
    ASSERT_EQ(VideoDecoder::kOk, status);

    is_read_pending_ = false;
    frame_read_ = frame;

    if (frame.get() && !frame->IsEndOfStream())
      num_decoded_frames_++;
  }

  enum CallbackResult {
    PENDING,
    OK,
    ABROTED,
    EOS
  };

  void ExpectReadResult(CallbackResult result) {
    switch (result) {
      case PENDING:
        EXPECT_TRUE(is_read_pending_);
        ASSERT_FALSE(frame_read_.get());
        break;
      case OK:
        EXPECT_FALSE(is_read_pending_);
        ASSERT_TRUE(frame_read_.get());
        EXPECT_FALSE(frame_read_->IsEndOfStream());
        break;
      case ABROTED:
        EXPECT_FALSE(is_read_pending_);
        EXPECT_FALSE(frame_read_.get());
        break;
      case EOS:
        EXPECT_FALSE(is_read_pending_);
        ASSERT_TRUE(frame_read_.get());
        EXPECT_TRUE(frame_read_->IsEndOfStream());
        break;
    }
  }

  void ReadOneFrame() {
    frame_read_ = NULL;
    is_read_pending_ = true;
    decoder_->Read(
        base::Bind(&FakeVideoDecoderTest::FrameReady, base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void ReadUntilEOS() {
    do {
      ReadOneFrame();
    } while (frame_read_.get() && !frame_read_->IsEndOfStream());
  }

  void EnterPendingReadState() {
    decoder_->HoldNextRead();
    ReadOneFrame();
    ExpectReadResult(PENDING);
  }

  void SatisfyRead() {
    decoder_->SatisfyRead();
    message_loop_.RunUntilIdle();
    ExpectReadResult(OK);
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

  // Callback for DemuxerStream::Read so that we can skip frames to trigger a
  // config change.
  void BufferReady(bool* config_changed,
                   DemuxerStream::Status status,
                   const scoped_refptr<DecoderBuffer>& buffer) {
    if (status == DemuxerStream::kConfigChanged)
      *config_changed = true;
  }

  void ChangeConfig() {
    bool config_changed = false;
    while (!config_changed) {
      demuxer_stream_->Read(base::Bind(&FakeVideoDecoderTest::BufferReady,
                                       base::Unretained(this),
                                       &config_changed));
      message_loop_.RunUntilIdle();
    }
  }

  void EnterPendingDemuxerReadState() {
    demuxer_stream_->HoldNextRead();
    ReadOneFrame();
  }

  void SatisfyDemuxerRead() {
    demuxer_stream_->SatisfyRead();
    message_loop_.RunUntilIdle();
  }

  void AbortDemuxerRead() {
    demuxer_stream_->Reset();
    message_loop_.RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  scoped_ptr<FakeVideoDecoder> decoder_;
  scoped_ptr<FakeDemuxerStream> demuxer_stream_;
  MockStatisticsCB statistics_cb_;
  int num_decoded_frames_;

  // Callback result/status.
  scoped_refptr<VideoFrame> frame_read_;
  bool is_read_pending_;
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
  EXPECT_EQ(kNumInputBuffers, num_decoded_frames_);
}

TEST_F(FakeVideoDecoderTest, Read_AbortedDemuxerRead) {
  Initialize();
  demuxer_stream_->HoldNextRead();
  ReadOneFrame();
  AbortDemuxerRead();
  ExpectReadResult(ABROTED);
}

TEST_F(FakeVideoDecoderTest, Read_DecodingDelay) {
  Initialize();

  while (demuxer_stream_->num_buffers_returned() < kNumInputBuffers) {
    ReadOneFrame();
    EXPECT_EQ(demuxer_stream_->num_buffers_returned(),
              num_decoded_frames_ + kDecodingDelay);
  }
}

TEST_F(FakeVideoDecoderTest, Read_ZeroDelay) {
  decoder_.reset(new FakeVideoDecoder(0));
  Initialize();

  while (demuxer_stream_->num_buffers_returned() < kNumInputBuffers) {
    ReadOneFrame();
    EXPECT_EQ(demuxer_stream_->num_buffers_returned(), num_decoded_frames_);
  }
}

TEST_F(FakeVideoDecoderTest, Read_Pending) {
  Initialize();
  EnterPendingReadState();
  SatisfyRead();
}

TEST_F(FakeVideoDecoderTest, Reinitialize) {
  Initialize();
  VideoDecoderConfig old_config = demuxer_stream_->video_decoder_config();
  ChangeConfig();
  VideoDecoderConfig new_config = demuxer_stream_->video_decoder_config();
  EXPECT_FALSE(new_config.Matches(old_config));
  Initialize();
}

// Reinitializing the decoder during the middle of the decoding process can
// cause dropped frames.
TEST_F(FakeVideoDecoderTest, Reinitialize_FrameDropped) {
  Initialize();
  ReadOneFrame();
  Initialize();
  ReadUntilEOS();
  EXPECT_LT(num_decoded_frames_, kNumInputBuffers);
}

TEST_F(FakeVideoDecoderTest, Reset) {
  Initialize();
  ReadOneFrame();
  ResetAndExpect(OK);
}

TEST_F(FakeVideoDecoderTest, Reset_DuringPendingDemuxerRead) {
  Initialize();
  EnterPendingDemuxerReadState();
  ResetAndExpect(PENDING);
  SatisfyDemuxerRead();
  ExpectReadResult(ABROTED);
}

TEST_F(FakeVideoDecoderTest, Reset_DuringPendingDemuxerRead_Aborted) {
  Initialize();
  EnterPendingDemuxerReadState();
  ResetAndExpect(PENDING);
  AbortDemuxerRead();
  ExpectReadResult(ABROTED);
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

TEST_F(FakeVideoDecoderTest, Stop_DuringPendingDemuxerRead) {
  Initialize();
  EnterPendingDemuxerReadState();
  StopAndExpect(PENDING);
  SatisfyDemuxerRead();
  ExpectReadResult(ABROTED);
}

TEST_F(FakeVideoDecoderTest, Stop_DuringPendingDemuxerRead_Aborted) {
  Initialize();
  EnterPendingDemuxerReadState();
  ResetAndExpect(PENDING);
  StopAndExpect(PENDING);
  SatisfyDemuxerRead();
  ExpectReadResult(ABROTED);
  ExpectResetResult(OK);
  ExpectStopResult(OK);
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

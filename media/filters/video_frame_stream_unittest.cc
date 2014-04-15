// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/decoder_stream.h"
#include "media/filters/fake_demuxer_stream.h"
#include "media/filters/fake_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

static const int kNumConfigs = 3;
static const int kNumBuffersInOneConfig = 5;
static const int kDecodingDelay = 7;

namespace media {

class VideoFrameStreamTest
    : public testing::Test,
      public testing::WithParamInterface<std::tr1::tuple<bool, bool> > {
 public:
  VideoFrameStreamTest()
      : demuxer_stream_(new FakeDemuxerStream(kNumConfigs,
                                              kNumBuffersInOneConfig,
                                              IsEncrypted())),
        decryptor_(new NiceMock<MockDecryptor>()),
        decoder_(
            new FakeVideoDecoder(kDecodingDelay, IsGetDecodeOutputEnabled())),
        is_initialized_(false),
        num_decoded_frames_(0),
        pending_initialize_(false),
        pending_read_(false),
        pending_reset_(false),
        pending_stop_(false),
        total_bytes_decoded_(0),
        has_no_key_(false) {
    ScopedVector<VideoDecoder> decoders;
    decoders.push_back(decoder_);

    video_frame_stream_.reset(new VideoFrameStream(
        message_loop_.message_loop_proxy(),
        decoders.Pass(),
        base::Bind(&VideoFrameStreamTest::SetDecryptorReadyCallback,
                   base::Unretained(this))));

    // Decryptor can only decrypt (not decrypt-and-decode) so that
    // DecryptingDemuxerStream will be used.
    EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
        .WillRepeatedly(RunCallback<1>(false));
    EXPECT_CALL(*decryptor_, Decrypt(_, _, _))
        .WillRepeatedly(Invoke(this, &VideoFrameStreamTest::Decrypt));
  }

  ~VideoFrameStreamTest() {
    DCHECK(!pending_initialize_);
    DCHECK(!pending_read_);
    DCHECK(!pending_reset_);
    DCHECK(!pending_stop_);

    if (is_initialized_)
      Stop();
    EXPECT_FALSE(is_initialized_);
  }

  bool IsEncrypted() {
    return std::tr1::get<0>(GetParam());
  }

  bool IsGetDecodeOutputEnabled() {
    return std::tr1::get<1>(GetParam());
  }

  MOCK_METHOD1(SetDecryptorReadyCallback, void(const media::DecryptorReadyCB&));

  void OnStatistics(const PipelineStatistics& statistics) {
    total_bytes_decoded_ += statistics.video_bytes_decoded;
  }

  void OnInitialized(bool success) {
    DCHECK(!pending_read_);
    DCHECK(!pending_reset_);
    DCHECK(pending_initialize_);
    pending_initialize_ = false;

    is_initialized_ = success;
    if (!success)
      decoder_ = NULL;
  }

  void InitializeVideoFrameStream() {
    pending_initialize_ = true;
    video_frame_stream_->Initialize(
        demuxer_stream_.get(),
        base::Bind(&VideoFrameStreamTest::OnStatistics, base::Unretained(this)),
        base::Bind(&VideoFrameStreamTest::OnInitialized,
                   base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  // Fake Decrypt() function used by DecryptingDemuxerStream. It does nothing
  // but removes the DecryptConfig to make the buffer unencrypted.
  void Decrypt(Decryptor::StreamType stream_type,
               const scoped_refptr<DecoderBuffer>& encrypted,
               const Decryptor::DecryptCB& decrypt_cb) {
    DCHECK(encrypted->decrypt_config());
    if (has_no_key_) {
      decrypt_cb.Run(Decryptor::kNoKey, NULL);
      return;
    }

    DCHECK_EQ(stream_type, Decryptor::kVideo);
    scoped_refptr<DecoderBuffer> decrypted = DecoderBuffer::CopyFrom(
        encrypted->data(), encrypted->data_size());
    decrypted->set_timestamp(encrypted->timestamp());
    decrypted->set_duration(encrypted->duration());
    decrypt_cb.Run(Decryptor::kSuccess, decrypted);
  }

  // Callback for VideoFrameStream::Read().
  void FrameReady(VideoFrameStream::Status status,
                  const scoped_refptr<VideoFrame>& frame) {
    DCHECK(pending_read_);
    // TODO(xhwang): Add test cases where the fake decoder returns error or
    // the fake demuxer aborts demuxer read.
    ASSERT_TRUE(status == VideoFrameStream::OK ||
                status == VideoFrameStream::ABORTED) << status;
    frame_read_ = frame;
    if (frame.get() && !frame->end_of_stream())
      num_decoded_frames_++;
    pending_read_ = false;
  }

  void OnReset() {
    DCHECK(!pending_read_);
    DCHECK(pending_reset_);
    pending_reset_ = false;
  }

  void OnStopped() {
    DCHECK(!pending_initialize_);
    DCHECK(!pending_read_);
    DCHECK(!pending_reset_);
    DCHECK(pending_stop_);
    pending_stop_ = false;
    is_initialized_ = false;
    decoder_ = NULL;
  }

  void ReadOneFrame() {
    frame_read_ = NULL;
    pending_read_ = true;
    video_frame_stream_->Read(base::Bind(
        &VideoFrameStreamTest::FrameReady, base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void ReadUntilPending() {
    do {
      ReadOneFrame();
    } while (!pending_read_);
  }

  enum PendingState {
    NOT_PENDING,
    DEMUXER_READ_NORMAL,
    DEMUXER_READ_CONFIG_CHANGE,
    SET_DECRYPTOR,
    DECRYPTOR_NO_KEY,
    DECODER_INIT,
    DECODER_REINIT,
    DECODER_DECODE,
    DECODER_RESET
  };

  void EnterPendingState(PendingState state) {
    DCHECK_NE(state, NOT_PENDING);
    switch (state) {
      case DEMUXER_READ_NORMAL:
        demuxer_stream_->HoldNextRead();
        ReadUntilPending();
        break;

      case DEMUXER_READ_CONFIG_CHANGE:
        demuxer_stream_->HoldNextConfigChangeRead();
        ReadUntilPending();
        break;

      case SET_DECRYPTOR:
        // Hold DecryptorReadyCB.
        EXPECT_CALL(*this, SetDecryptorReadyCallback(_))
            .Times(2);
        // Initialize will fail because no decryptor is available.
        InitializeVideoFrameStream();
        break;

      case DECRYPTOR_NO_KEY:
        EXPECT_CALL(*this, SetDecryptorReadyCallback(_))
            .WillRepeatedly(RunCallback<0>(decryptor_.get()));
        has_no_key_ = true;
        ReadOneFrame();
        break;

      case DECODER_INIT:
        EXPECT_CALL(*this, SetDecryptorReadyCallback(_))
            .WillRepeatedly(RunCallback<0>(decryptor_.get()));
        decoder_->HoldNextInit();
        InitializeVideoFrameStream();
        break;

      case DECODER_REINIT:
        decoder_->HoldNextInit();
        ReadUntilPending();
        break;

      case DECODER_DECODE:
        decoder_->HoldNextDecode();
        ReadUntilPending();
        break;

      case DECODER_RESET:
        decoder_->HoldNextReset();
        pending_reset_ = true;
        video_frame_stream_->Reset(base::Bind(&VideoFrameStreamTest::OnReset,
                                              base::Unretained(this)));
        message_loop_.RunUntilIdle();
        break;

      case NOT_PENDING:
        NOTREACHED();
        break;
    }
  }

  void SatisfyPendingCallback(PendingState state) {
    DCHECK_NE(state, NOT_PENDING);
    switch (state) {
      case DEMUXER_READ_NORMAL:
      case DEMUXER_READ_CONFIG_CHANGE:
        demuxer_stream_->SatisfyRead();
        break;

      // These two cases are only interesting to test during
      // VideoFrameStream::Stop().  There's no need to satisfy a callback.
      case SET_DECRYPTOR:
      case DECRYPTOR_NO_KEY:
        NOTREACHED();
        break;

      case DECODER_INIT:
        decoder_->SatisfyInit();
        break;

      case DECODER_REINIT:
        decoder_->SatisfyInit();
        break;

      case DECODER_DECODE:
        decoder_->SatisfyDecode();
        break;

      case DECODER_RESET:
        decoder_->SatisfyReset();
        break;

      case NOT_PENDING:
        NOTREACHED();
        break;
    }

    message_loop_.RunUntilIdle();
  }

  void Initialize() {
    EnterPendingState(DECODER_INIT);
    SatisfyPendingCallback(DECODER_INIT);
  }

  void Read() {
    EnterPendingState(DECODER_DECODE);
    SatisfyPendingCallback(DECODER_DECODE);
  }

  void Reset() {
    EnterPendingState(DECODER_RESET);
    SatisfyPendingCallback(DECODER_RESET);
  }

  void Stop() {
    // Check that the pipeline statistics callback was fired correctly.
    EXPECT_EQ(decoder_->total_bytes_decoded(), total_bytes_decoded_);
    pending_stop_ = true;
    video_frame_stream_->Stop(base::Bind(&VideoFrameStreamTest::OnStopped,
                                         base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  base::MessageLoop message_loop_;

  scoped_ptr<VideoFrameStream> video_frame_stream_;
  scoped_ptr<FakeDemuxerStream> demuxer_stream_;
  // Use NiceMock since we don't care about most of calls on the decryptor,
  // e.g. RegisterNewKeyCB().
  scoped_ptr<NiceMock<MockDecryptor> > decryptor_;
  FakeVideoDecoder* decoder_;  // Owned by |video_frame_stream_|.

  bool is_initialized_;
  int num_decoded_frames_;
  bool pending_initialize_;
  bool pending_read_;
  bool pending_reset_;
  bool pending_stop_;
  int total_bytes_decoded_;
  scoped_refptr<VideoFrame> frame_read_;

  // Decryptor has no key to decrypt a frame.
  bool has_no_key_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoFrameStreamTest);
};

INSTANTIATE_TEST_CASE_P(Clear, VideoFrameStreamTest,
  testing::Combine(testing::Values(false), testing::Values(false)));
INSTANTIATE_TEST_CASE_P(Clear_GetDecodeOutput, VideoFrameStreamTest,
  testing::Combine(testing::Values(false), testing::Values(true)));
INSTANTIATE_TEST_CASE_P(Encrypted, VideoFrameStreamTest,
  testing::Combine(testing::Values(true), testing::Values(false)));
INSTANTIATE_TEST_CASE_P(Encrypted_GetDecodeOutput, VideoFrameStreamTest,
  testing::Combine(testing::Values(true), testing::Values(true)));

TEST_P(VideoFrameStreamTest, Initialization) {
  Initialize();
}

TEST_P(VideoFrameStreamTest, ReadOneFrame) {
  Initialize();
  Read();
}

TEST_P(VideoFrameStreamTest, ReadAllFrames) {
  Initialize();
  do {
    Read();
  } while (frame_read_.get() && !frame_read_->end_of_stream());

  const int total_num_frames = kNumConfigs * kNumBuffersInOneConfig;
  DCHECK_EQ(num_decoded_frames_, total_num_frames);
}

TEST_P(VideoFrameStreamTest, Read_AfterReset) {
  Initialize();
  Reset();
  Read();
  Reset();
  Read();
}

// No Reset() before initialization is successfully completed.

TEST_P(VideoFrameStreamTest, Reset_AfterInitialization) {
  Initialize();
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_DuringReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
  // VideoDecoder::Reset() is not called when we reset during reinitialization.
  pending_reset_ = true;
  video_frame_stream_->Reset(
      base::Bind(&VideoFrameStreamTest::OnReset, base::Unretained(this)));
  SatisfyPendingCallback(DECODER_REINIT);
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_AfterReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
  SatisfyPendingCallback(DECODER_REINIT);
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_DuringDemuxerRead_Normal) {
  Initialize();
  EnterPendingState(DEMUXER_READ_NORMAL);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DEMUXER_READ_NORMAL);
  SatisfyPendingCallback(DECODER_RESET);
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_DuringDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DECODER_RESET);
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_DuringNormalDecoderDecode) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DECODER_DECODE);
  SatisfyPendingCallback(DECODER_RESET);
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_AfterNormalRead) {
  Initialize();
  Read();
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_AfterDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_DuringNoKeyRead) {
  Initialize();
  EnterPendingState(DECRYPTOR_NO_KEY);
  Reset();
}

TEST_P(VideoFrameStreamTest, Stop_BeforeInitialization) {
  pending_stop_ = true;
  video_frame_stream_->Stop(
      base::Bind(&VideoFrameStreamTest::OnStopped, base::Unretained(this)));
  message_loop_.RunUntilIdle();
}

TEST_P(VideoFrameStreamTest, Stop_DuringSetDecryptor) {
  if (!IsEncrypted()) {
    DVLOG(1) << "SetDecryptor test only runs when the stream is encrytped.";
    return;
  }

  EnterPendingState(SET_DECRYPTOR);
  pending_stop_ = true;
  video_frame_stream_->Stop(
      base::Bind(&VideoFrameStreamTest::OnStopped, base::Unretained(this)));
  message_loop_.RunUntilIdle();
}

TEST_P(VideoFrameStreamTest, Stop_DuringInitialization) {
  EnterPendingState(DECODER_INIT);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterInitialization) {
  Initialize();
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
  SatisfyPendingCallback(DECODER_REINIT);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringDemuxerRead_Normal) {
  Initialize();
  EnterPendingState(DEMUXER_READ_NORMAL);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringNormalDecoderDecode) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterNormalRead) {
  Initialize();
  Read();
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterConfigChangeRead) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringNoKeyRead) {
  Initialize();
  EnterPendingState(DECRYPTOR_NO_KEY);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_RESET);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterReset) {
  Initialize();
  Reset();
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_DuringRead_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterRead_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DECODER_DECODE);
  Stop();
}

TEST_P(VideoFrameStreamTest, Stop_AfterRead_AfterReset) {
  Initialize();
  Read();
  Reset();
  Stop();
}

}  // namespace media

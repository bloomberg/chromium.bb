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
using ::testing::AnyNumber;
using ::testing::Assign;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

static const int kNumConfigs = 3;
static const int kNumBuffersInOneConfig = 5;

// Use anonymous namespace here to prevent the actions to be defined multiple
// times across multiple test files. Sadly we can't use static for them.
namespace {

ACTION_P3(ExecuteCallbackWithVerifier, decryptor, done_cb, verifier) {
  // verifier must be called first since |done_cb| call will invoke it as well.
  verifier->RecordACalled();
  arg0.Run(decryptor, done_cb);
}

ACTION_P(ReportCallback, verifier) {
  verifier->RecordBCalled();
}

}  // namespace

namespace media {

struct VideoFrameStreamTestParams {
  VideoFrameStreamTestParams(bool is_encrypted,
                             int decoding_delay,
                             int parallel_decoding)
      : is_encrypted(is_encrypted),
        decoding_delay(decoding_delay),
        parallel_decoding(parallel_decoding) {}

  bool is_encrypted;
  int decoding_delay;
  int parallel_decoding;
};

class VideoFrameStreamTest
    : public testing::Test,
      public testing::WithParamInterface<VideoFrameStreamTestParams> {
 public:
  VideoFrameStreamTest()
      : demuxer_stream_(new FakeDemuxerStream(kNumConfigs,
                                              kNumBuffersInOneConfig,
                                              GetParam().is_encrypted)),
        decryptor_(new NiceMock<MockDecryptor>()),
        decoder_(new FakeVideoDecoder(GetParam().decoding_delay,
                                      GetParam().parallel_decoding)),
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
                   base::Unretained(this)),
        new MediaLog()));

    // Decryptor can only decrypt (not decrypt-and-decode) so that
    // DecryptingDemuxerStream will be used.
    EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
        .WillRepeatedly(RunCallback<1>(false));
    EXPECT_CALL(*decryptor_, Decrypt(_, _, _))
        .WillRepeatedly(Invoke(this, &VideoFrameStreamTest::Decrypt));
  }

  ~VideoFrameStreamTest() {
    // Check that the pipeline statistics callback was fired correctly.
    if (decoder_)
      EXPECT_EQ(decoder_->total_bytes_decoded(), total_bytes_decoded_);

    is_initialized_ = false;
    decoder_ = NULL;
    video_frame_stream_.reset();
    message_loop_.RunUntilIdle();

    DCHECK(!pending_initialize_);
    DCHECK(!pending_read_);
    DCHECK(!pending_reset_);
    DCHECK(!pending_stop_);
  }

  MOCK_METHOD1(OnNewSpliceBuffer, void(base::TimeDelta));
  MOCK_METHOD1(SetDecryptorReadyCallback, void(const media::DecryptorReadyCB&));
  MOCK_METHOD1(DecryptorSet, void(bool));

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
        false,
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
    scoped_refptr<DecoderBuffer> decrypted =
        DecoderBuffer::CopyFrom(encrypted->data(), encrypted->data_size());
    decrypted->set_timestamp(encrypted->timestamp());
    decrypted->set_duration(encrypted->duration());
    decrypt_cb.Run(Decryptor::kSuccess, decrypted);
  }

  // Callback for VideoFrameStream::Read().
  void FrameReady(VideoFrameStream::Status status,
                  const scoped_refptr<VideoFrame>& frame) {
    DCHECK(pending_read_);
    frame_read_ = frame;
    last_read_status_ = status;
    if (frame.get() && !frame->end_of_stream())
      num_decoded_frames_++;
    pending_read_ = false;
  }

  void FrameReadyHoldDemuxer(VideoFrameStream::Status status,
                             const scoped_refptr<VideoFrame>& frame) {
    FrameReady(status, frame);

  }

  void OnReset() {
    DCHECK(!pending_read_);
    DCHECK(pending_reset_);
    pending_reset_ = false;
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

  void ReadAllFrames() {
    do {
      ReadOneFrame();
    } while (frame_read_.get() && !frame_read_->end_of_stream());

    const int total_num_frames = kNumConfigs * kNumBuffersInOneConfig;
    DCHECK_EQ(num_decoded_frames_, total_num_frames);
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

  void ExpectDecryptorNotification() {
    EXPECT_CALL(*this, SetDecryptorReadyCallback(_))
        .WillRepeatedly(ExecuteCallbackWithVerifier(
            decryptor_.get(),
            base::Bind(&VideoFrameStreamTest::DecryptorSet,
                       base::Unretained(this)),
            &verifier_));
    EXPECT_CALL(*this, DecryptorSet(true))
        .WillRepeatedly(ReportCallback(&verifier_));
  }

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
        ExpectDecryptorNotification();
        has_no_key_ = true;
        ReadOneFrame();
        break;

      case DECODER_INIT:
        ExpectDecryptorNotification();
        decoder_->HoldNextInit();
        InitializeVideoFrameStream();
        break;

      case DECODER_REINIT:
        decoder_->HoldNextInit();
        ReadUntilPending();
        break;

      case DECODER_DECODE:
        decoder_->HoldDecode();
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
      // VideoFrameStream destruction.  There's no need to satisfy a callback.
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
  VideoFrameStream::Status last_read_status_;

  // Decryptor has no key to decrypt a frame.
  bool has_no_key_;

  CallbackPairChecker verifier_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoFrameStreamTest);
};

INSTANTIATE_TEST_CASE_P(
    Clear,
    VideoFrameStreamTest,
    ::testing::Values(
        VideoFrameStreamTestParams(false, 0, 1),
        VideoFrameStreamTestParams(false, 3, 1),
        VideoFrameStreamTestParams(false, 7, 1)));

INSTANTIATE_TEST_CASE_P(
    Encrypted,
    VideoFrameStreamTest,
    ::testing::Values(
        VideoFrameStreamTestParams(true, 7, 1)));

INSTANTIATE_TEST_CASE_P(
    Clear_Parallel,
    VideoFrameStreamTest,
    ::testing::Values(
        VideoFrameStreamTestParams(false, 0, 3),
        VideoFrameStreamTestParams(false, 2, 3)));


TEST_P(VideoFrameStreamTest, Initialization) {
  Initialize();
}

TEST_P(VideoFrameStreamTest, ReadOneFrame) {
  Initialize();
  Read();
}

TEST_P(VideoFrameStreamTest, ReadAllFrames) {
  Initialize();
  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest, Read_AfterReset) {
  Initialize();
  Reset();
  Read();
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Read_BlockedDemuxer) {
  Initialize();
  demuxer_stream_->HoldNextRead();
  ReadOneFrame();
  EXPECT_TRUE(pending_read_);

  int demuxed_buffers = 0;

  // Pass frames from the demuxer to the VideoFrameStream until the first read
  // request is satisfied.
  while (pending_read_) {
    ++demuxed_buffers;
    demuxer_stream_->SatisfyReadAndHoldNext();
    message_loop_.RunUntilIdle();
  }

  EXPECT_EQ(std::min(GetParam().decoding_delay + 1, kNumBuffersInOneConfig + 1),
            demuxed_buffers);

  // At this point the stream is waiting on read from the demuxer, but there is
  // no pending read from the stream. The stream should be blocked if we try
  // reading from it again.
  ReadUntilPending();

  demuxer_stream_->SatisfyRead();
  message_loop_.RunUntilIdle();
  EXPECT_FALSE(pending_read_);
}

TEST_P(VideoFrameStreamTest, Read_BlockedDemuxerAndDecoder) {
  // Test applies only when the decoder allows multiple parallel requests.
  if (GetParam().parallel_decoding == 1)
    return;

  Initialize();
  demuxer_stream_->HoldNextRead();
  decoder_->HoldDecode();
  ReadOneFrame();
  EXPECT_TRUE(pending_read_);

  int demuxed_buffers = 0;

  // Pass frames from the demuxer to the VideoFrameStream until the first read
  // request is satisfied, while always keeping one decode request pending.
  while (pending_read_) {
    ++demuxed_buffers;
    demuxer_stream_->SatisfyReadAndHoldNext();
    message_loop_.RunUntilIdle();

    // Always keep one decode request pending.
    if (demuxed_buffers > 1) {
      decoder_->SatisfySingleDecode();
      message_loop_.RunUntilIdle();
    }
  }

  ReadUntilPending();
  EXPECT_TRUE(pending_read_);

  // Unblocking one decode request should unblock read even when demuxer is
  // still blocked.
  decoder_->SatisfySingleDecode();
  message_loop_.RunUntilIdle();
  EXPECT_FALSE(pending_read_);

  // Stream should still be blocked on the demuxer after unblocking the decoder.
  decoder_->SatisfyDecode();
  ReadUntilPending();
  EXPECT_TRUE(pending_read_);

  // Verify that the stream has returned all frames that have been demuxed,
  // accounting for the decoder delay.
  EXPECT_EQ(demuxed_buffers - GetParam().decoding_delay, num_decoded_frames_);

  // Unblocking the demuxer will unblock the stream.
  demuxer_stream_->SatisfyRead();
  message_loop_.RunUntilIdle();
  EXPECT_FALSE(pending_read_);
}

TEST_P(VideoFrameStreamTest, Read_DuringEndOfStreamDecode) {
  // Test applies only when the decoder allows multiple parallel requests, and
  // they are not satisfied in a single batch.
  if (GetParam().parallel_decoding == 1 || GetParam().decoding_delay != 0)
    return;

  Initialize();
  decoder_->HoldDecode();

  // Read all of the frames up to end of stream. Since parallel decoding is
  // enabled, the end of stream buffer will be sent to the decoder immediately,
  // but we don't satisfy it yet.
  for (int configuration = 0; configuration < kNumConfigs; configuration++) {
    for (int frame = 0; frame < kNumBuffersInOneConfig; frame++) {
      ReadOneFrame();
      while (pending_read_) {
        decoder_->SatisfySingleDecode();
        message_loop_.RunUntilIdle();
      }
    }
  }

  // Read() again. The callback must be delayed until the decode completes.
  ReadOneFrame();
  ASSERT_TRUE(pending_read_);

  // Satisfy decoding of the end of stream buffer. The read should complete.
  decoder_->SatisfySingleDecode();
  message_loop_.RunUntilIdle();
  ASSERT_FALSE(pending_read_);
  EXPECT_EQ(last_read_status_, VideoFrameStream::OK);

  // The read output should indicate end of stream.
  ASSERT_TRUE(frame_read_.get());
  EXPECT_TRUE(frame_read_->end_of_stream());
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

TEST_P(VideoFrameStreamTest, Reset_AfterNormalReadWithActiveSplice) {
  video_frame_stream_->set_splice_observer(base::Bind(
      &VideoFrameStreamTest::OnNewSpliceBuffer, base::Unretained(this)));
  Initialize();

  // Send buffers with a splice timestamp, which sets the active splice flag.
  const base::TimeDelta splice_timestamp = base::TimeDelta();
  demuxer_stream_->set_splice_timestamp(splice_timestamp);
  EXPECT_CALL(*this, OnNewSpliceBuffer(splice_timestamp)).Times(AnyNumber());
  Read();

  // Issue an explicit Reset() and clear the splice timestamp.
  Reset();
  demuxer_stream_->set_splice_timestamp(kNoTimestamp());

  // Ensure none of the upcoming calls indicate they have a splice timestamp.
  EXPECT_CALL(*this, OnNewSpliceBuffer(_)).Times(0);
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_AfterDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_AfterEndOfStream) {
  Initialize();
  ReadAllFrames();
  Reset();
  num_decoded_frames_ = 0;
  demuxer_stream_->SeekToStart();
  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest, Reset_DuringNoKeyRead) {
  Initialize();
  EnterPendingState(DECRYPTOR_NO_KEY);
  Reset();
}

// In the following Destroy_* tests, |video_frame_stream_| is destroyed in
// VideoFrameStreamTest dtor.

TEST_P(VideoFrameStreamTest, Destroy_BeforeInitialization) {
}

TEST_P(VideoFrameStreamTest, Destroy_DuringSetDecryptor) {
  if (!GetParam().is_encrypted) {
    DVLOG(1) << "SetDecryptor test only runs when the stream is encrytped.";
    return;
  }

  EnterPendingState(SET_DECRYPTOR);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringInitialization) {
  EnterPendingState(DECODER_INIT);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterInitialization) {
  Initialize();
}

TEST_P(VideoFrameStreamTest, Destroy_DuringReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
  SatisfyPendingCallback(DECODER_REINIT);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringDemuxerRead_Normal) {
  Initialize();
  EnterPendingState(DEMUXER_READ_NORMAL);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringNormalDecoderDecode) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterNormalRead) {
  Initialize();
  Read();
}

TEST_P(VideoFrameStreamTest, Destroy_AfterConfigChangeRead) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringNoKeyRead) {
  Initialize();
  EnterPendingState(DECRYPTOR_NO_KEY);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_RESET);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterReset) {
  Initialize();
  Reset();
}

TEST_P(VideoFrameStreamTest, Destroy_DuringRead_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterRead_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DECODER_DECODE);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterRead_AfterReset) {
  Initialize();
  Read();
  Reset();
}

TEST_P(VideoFrameStreamTest, DecoderErrorWhenReading) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  decoder_->SimulateError();
  message_loop_.RunUntilIdle();
  ASSERT_FALSE(pending_read_);
  ASSERT_EQ(VideoFrameStream::DECODE_ERROR, last_read_status_);
}

TEST_P(VideoFrameStreamTest, DecoderErrorWhenNotReading) {
  Initialize();

  decoder_->HoldDecode();
  ReadOneFrame();
  EXPECT_TRUE(pending_read_);

  // Satisfy decode requests until we get the first frame out.
  while (pending_read_) {
    decoder_->SatisfySingleDecode();
    message_loop_.RunUntilIdle();
  }

  // Trigger an error in the decoding.
  decoder_->SimulateError();

  // The error must surface from Read() as DECODE_ERROR.
  while (last_read_status_ == VideoFrameStream::OK) {
    ReadOneFrame();
    message_loop_.RunUntilIdle();
    EXPECT_FALSE(pending_read_);
  }
  EXPECT_EQ(VideoFrameStream::DECODE_ERROR, last_read_status_);
}

}  // namespace media

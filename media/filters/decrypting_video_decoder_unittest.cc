// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/filters/decrypting_video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::IsNull;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

static const uint8 kFakeKeyId[] = { 0x4b, 0x65, 0x79, 0x20, 0x49, 0x44 };
static const uint8 kFakeIv[DecryptConfig::kDecryptionKeySize] = { 0 };

// Create a fake non-empty encrypted buffer.
static scoped_refptr<DecoderBuffer> CreateFakeEncryptedBuffer() {
  const int buffer_size = 16;  // Need a non-empty buffer;
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(buffer_size));
  buffer->set_decrypt_config(scoped_ptr<DecryptConfig>(new DecryptConfig(
      std::string(reinterpret_cast<const char*>(kFakeKeyId),
                  arraysize(kFakeKeyId)),
      std::string(reinterpret_cast<const char*>(kFakeIv), arraysize(kFakeIv)),
      std::vector<SubsampleEntry>())));
  return buffer;
}

// Use anonymous namespace here to prevent the actions to be defined multiple
// times across multiple test files. Sadly we can't use static for them.
namespace {

ACTION_P(RunCallbackIfNotNull, param) {
  if (!arg0.is_null())
    arg0.Run(param);
}

ACTION_P2(ResetAndRunCallback, callback, param) {
  base::ResetAndReturn(callback).Run(param);
}

MATCHER(IsEndOfStream, "end of stream") {
  return (arg->end_of_stream());
}

}  // namespace

class DecryptingVideoDecoderTest : public testing::Test {
 public:
  DecryptingVideoDecoderTest()
      : decoder_(new DecryptingVideoDecoder(
            message_loop_.message_loop_proxy(),
            base::Bind(
                &DecryptingVideoDecoderTest::RequestDecryptorNotification,
                base::Unretained(this)))),
        decryptor_(new StrictMock<MockDecryptor>()),
        encrypted_buffer_(CreateFakeEncryptedBuffer()),
        decoded_video_frame_(VideoFrame::CreateBlackFrame(
            TestVideoConfig::NormalCodedSize())),
        null_video_frame_(scoped_refptr<VideoFrame>()),
        end_of_stream_video_frame_(VideoFrame::CreateEOSFrame()) {
    EXPECT_CALL(*this, RequestDecryptorNotification(_))
        .WillRepeatedly(RunCallbackIfNotNull(decryptor_.get()));
  }

  virtual ~DecryptingVideoDecoderTest() {
    Stop();
  }

  // Initializes the |decoder_| and expects |status|. Note the initialization
  // can succeed or fail.
  void InitializeAndExpectStatus(const VideoDecoderConfig& config,
                                 PipelineStatus status) {
    decoder_->Initialize(config, NewExpectedStatusCB(status));
    message_loop_.RunUntilIdle();
  }

  // Initialize the |decoder_| and expects it to succeed.
  void Initialize() {
    EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kVideo, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));

    InitializeAndExpectStatus(TestVideoConfig::NormalEncrypted(), PIPELINE_OK);
  }

  // Reinitialize the |decoder_| and expects it to succeed.
  void Reinitialize() {
    EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kVideo));
    EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kVideo, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));

    InitializeAndExpectStatus(TestVideoConfig::LargeEncrypted(), PIPELINE_OK);
  }

  void ReadAndExpectFrameReadyWith(
      const scoped_refptr<DecoderBuffer>& buffer,
      VideoDecoder::Status status,
      const scoped_refptr<VideoFrame>& video_frame) {
    if (status != VideoDecoder::kOk)
      EXPECT_CALL(*this, FrameReady(status, IsNull()));
    else if (video_frame.get() && video_frame->end_of_stream())
      EXPECT_CALL(*this, FrameReady(status, IsEndOfStream()));
    else
      EXPECT_CALL(*this, FrameReady(status, video_frame));

    decoder_->Decode(buffer,
                     base::Bind(&DecryptingVideoDecoderTest::FrameReady,
                                base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  // Sets up expectations and actions to put DecryptingVideoDecoder in an
  // active normal decoding state.
  void EnterNormalDecodingState() {
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
        .WillOnce(RunCallback<1>(Decryptor::kSuccess, decoded_video_frame_))
        .WillRepeatedly(RunCallback<1>(Decryptor::kNeedMoreData,
                                       scoped_refptr<VideoFrame>()));
    ReadAndExpectFrameReadyWith(
        encrypted_buffer_, VideoDecoder::kOk, decoded_video_frame_);
  }

  // Sets up expectations and actions to put DecryptingVideoDecoder in an end
  // of stream state. This function must be called after
  // EnterNormalDecodingState() to work.
  void EnterEndOfStreamState() {
    ReadAndExpectFrameReadyWith(DecoderBuffer::CreateEOSBuffer(),
                                VideoDecoder::kOk,
                                end_of_stream_video_frame_);
  }

  // Make the video decode callback pending by saving and not firing it.
  void EnterPendingDecodeState() {
    EXPECT_TRUE(pending_video_decode_cb_.is_null());
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(encrypted_buffer_, _))
        .WillOnce(SaveArg<1>(&pending_video_decode_cb_));

    decoder_->Decode(encrypted_buffer_,
                     base::Bind(&DecryptingVideoDecoderTest::FrameReady,
                                base::Unretained(this)));
    message_loop_.RunUntilIdle();
    // Make sure the Decode() on the decoder triggers a DecryptAndDecode() on
    // the decryptor.
    EXPECT_FALSE(pending_video_decode_cb_.is_null());
  }

  void EnterWaitingForKeyState() {
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
        .WillRepeatedly(RunCallback<1>(Decryptor::kNoKey, null_video_frame_));
    decoder_->Decode(encrypted_buffer_,
                     base::Bind(&DecryptingVideoDecoderTest::FrameReady,
                                base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void AbortPendingVideoDecodeCB() {
    if (!pending_video_decode_cb_.is_null()) {
      base::ResetAndReturn(&pending_video_decode_cb_).Run(
          Decryptor::kSuccess, scoped_refptr<VideoFrame>(NULL));
    }
  }

  void AbortAllPendingCBs() {
    if (!pending_init_cb_.is_null()) {
      ASSERT_TRUE(pending_video_decode_cb_.is_null());
      base::ResetAndReturn(&pending_init_cb_).Run(false);
      return;
    }

    AbortPendingVideoDecodeCB();
  }

  void Reset() {
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kVideo))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingVideoDecoderTest::AbortPendingVideoDecodeCB));

    decoder_->Reset(NewExpectedClosure());
    message_loop_.RunUntilIdle();
  }

  void Stop() {
    EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kVideo))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingVideoDecoderTest::AbortAllPendingCBs));

    decoder_->Stop(NewExpectedClosure());
    message_loop_.RunUntilIdle();
  }

  MOCK_METHOD1(RequestDecryptorNotification, void(const DecryptorReadyCB&));

  MOCK_METHOD2(FrameReady, void(VideoDecoder::Status,
                                const scoped_refptr<VideoFrame>&));

  base::MessageLoop message_loop_;
  scoped_ptr<DecryptingVideoDecoder> decoder_;
  scoped_ptr<StrictMock<MockDecryptor> > decryptor_;

  Decryptor::DecoderInitCB pending_init_cb_;
  Decryptor::NewKeyCB key_added_cb_;
  Decryptor::VideoDecodeCB pending_video_decode_cb_;

  // Constant buffer/frames.
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<VideoFrame> decoded_video_frame_;
  scoped_refptr<VideoFrame> null_video_frame_;
  scoped_refptr<VideoFrame> end_of_stream_video_frame_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecryptingVideoDecoderTest);
};

TEST_F(DecryptingVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(DecryptingVideoDecoderTest, Initialize_NullDecryptor) {
  EXPECT_CALL(*this, RequestDecryptorNotification(_))
      .WillRepeatedly(RunCallbackIfNotNull(static_cast<Decryptor*>(NULL)));
  InitializeAndExpectStatus(TestVideoConfig::NormalEncrypted(),
                            DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(DecryptingVideoDecoderTest, Initialize_Failure) {
  EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
      .WillRepeatedly(RunCallback<1>(false));
  EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kVideo, _))
      .WillRepeatedly(SaveArg<1>(&key_added_cb_));

  InitializeAndExpectStatus(TestVideoConfig::NormalEncrypted(),
                            DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(DecryptingVideoDecoderTest, Reinitialize_Normal) {
  Initialize();
  EnterNormalDecodingState();
  Reinitialize();
}

TEST_F(DecryptingVideoDecoderTest, Reinitialize_Failure) {
  Initialize();
  EnterNormalDecodingState();

  EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kVideo));
  EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
      .WillOnce(RunCallback<1>(false));

  // Reinitialize() expects the reinitialization to succeed. Call
  // InitializeAndExpectStatus() directly to test the reinitialization failure.
  InitializeAndExpectStatus(TestVideoConfig::NormalEncrypted(),
                            DECODER_ERROR_NOT_SUPPORTED);
}

// Test normal decrypt and decode case.
TEST_F(DecryptingVideoDecoderTest, DecryptAndDecode_Normal) {
  Initialize();
  EnterNormalDecodingState();
}

// Test the case where the decryptor returns error when doing decrypt and
// decode.
TEST_F(DecryptingVideoDecoderTest, DecryptAndDecode_DecodeError) {
  Initialize();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kError,
                                     scoped_refptr<VideoFrame>(NULL)));

  ReadAndExpectFrameReadyWith(
      encrypted_buffer_, VideoDecoder::kDecodeError, null_video_frame_);

  // After a decode error occurred, all following decode returns kDecodeError.
  ReadAndExpectFrameReadyWith(
      encrypted_buffer_, VideoDecoder::kDecodeError, null_video_frame_);
}

// Test the case where the decryptor returns kNeedMoreData to ask for more
// buffers before it can produce a frame.
TEST_F(DecryptingVideoDecoderTest, DecryptAndDecode_NeedMoreData) {
  Initialize();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillOnce(RunCallback<1>(Decryptor::kNeedMoreData,
                             scoped_refptr<VideoFrame>()))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess,
                                     decoded_video_frame_));

  ReadAndExpectFrameReadyWith(
      encrypted_buffer_, VideoDecoder::kNotEnoughData, decoded_video_frame_);
  ReadAndExpectFrameReadyWith(
      encrypted_buffer_, VideoDecoder::kOk, decoded_video_frame_);
}

// Test the case where the decryptor receives end-of-stream buffer.
TEST_F(DecryptingVideoDecoderTest, DecryptAndDecode_EndOfStream) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
}

// Test the case where the a key is added when the decryptor is in
// kWaitingForKey state.
TEST_F(DecryptingVideoDecoderTest, KeyAdded_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess,
                                     decoded_video_frame_));
  EXPECT_CALL(*this, FrameReady(VideoDecoder::kOk, decoded_video_frame_));
  key_added_cb_.Run();
  message_loop_.RunUntilIdle();
}

// Test the case where the a key is added when the decryptor is in
// kPendingDecode state.
TEST_F(DecryptingVideoDecoderTest, KeyAdded_DruingPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess,
                                     decoded_video_frame_));
  EXPECT_CALL(*this, FrameReady(VideoDecoder::kOk, decoded_video_frame_));
  // The video decode callback is returned after the correct decryption key is
  // added.
  key_added_cb_.Run();
  base::ResetAndReturn(&pending_video_decode_cb_).Run(Decryptor::kNoKey,
                                                      null_video_frame_);
  message_loop_.RunUntilIdle();
}

// Test resetting when the decoder is in kIdle state but has not decoded any
// frame.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringIdleAfterInitialization) {
  Initialize();
  Reset();
}

// Test resetting when the decoder is in kIdle state after it has decoded one
// frame.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringIdleAfterDecodedOneFrame) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
}

// Test resetting when the decoder is in kPendingDecode state.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*this, FrameReady(VideoDecoder::kAborted, IsNull()));

  Reset();
}

// Test resetting when the decoder is in kWaitingForKey state.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, FrameReady(VideoDecoder::kAborted, IsNull()));

  Reset();
}

// Test resetting when the decoder has hit end of stream and is in
// kDecodeFinished state.
TEST_F(DecryptingVideoDecoderTest, Reset_AfterDecodeFinished) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
  Reset();
}

// Test resetting after the decoder has been reset.
TEST_F(DecryptingVideoDecoderTest, Reset_AfterReset) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
  Reset();
}

// Test stopping when the decoder is in kDecryptorRequested state.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringDecryptorRequested) {
  DecryptorReadyCB decryptor_ready_cb;
  EXPECT_CALL(*this, RequestDecryptorNotification(_))
      .WillOnce(SaveArg<0>(&decryptor_ready_cb));
  decoder_->Initialize(TestVideoConfig::NormalEncrypted(),
                       NewExpectedStatusCB(DECODER_ERROR_NOT_SUPPORTED));
  message_loop_.RunUntilIdle();
  // |decryptor_ready_cb| is saved but not called here.
  EXPECT_FALSE(decryptor_ready_cb.is_null());

  // During stop, RequestDecryptorNotification() should be called with a NULL
  // callback to cancel the |decryptor_ready_cb|.
  EXPECT_CALL(*this, RequestDecryptorNotification(IsNullCallback()))
      .WillOnce(ResetAndRunCallback(&decryptor_ready_cb,
                                    reinterpret_cast<Decryptor*>(NULL)));
  Stop();
}

// Test stopping when the decoder is in kPendingDecoderInit state.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringPendingDecoderInit) {
  EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
      .WillOnce(SaveArg<1>(&pending_init_cb_));

  InitializeAndExpectStatus(TestVideoConfig::NormalEncrypted(),
                            DECODER_ERROR_NOT_SUPPORTED);
  EXPECT_FALSE(pending_init_cb_.is_null());

  Stop();
}

// Test stopping when the decoder is in kIdle state but has not decoded any
// frame.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringIdleAfterInitialization) {
  Initialize();
  Stop();
}

// Test stopping when the decoder is in kIdle state after it has decoded one
// frame.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringIdleAfterDecodedOneFrame) {
  Initialize();
  EnterNormalDecodingState();
  Stop();
}

// Test stopping when the decoder is in kPendingDecode state.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*this, FrameReady(VideoDecoder::kAborted, IsNull()));

  Stop();
}

// Test stopping when the decoder is in kWaitingForKey state.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, FrameReady(VideoDecoder::kAborted, IsNull()));

  Stop();
}

// Test stopping when the decoder has hit end of stream and is in
// kDecodeFinished state.
TEST_F(DecryptingVideoDecoderTest, Stop_AfterDecodeFinished) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
  Stop();
}

// Test stopping when there is a pending reset on the decoder.
// Reset is pending because it cannot complete when the video decode callback
// is pending.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringPendingReset) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kVideo));
  EXPECT_CALL(*this, FrameReady(VideoDecoder::kAborted, IsNull()));

  decoder_->Reset(NewExpectedClosure());
  Stop();
}

// Test stopping after the decoder has been reset.
TEST_F(DecryptingVideoDecoderTest, Stop_AfterReset) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
  Stop();
}

// Test stopping after the decoder has been stopped.
TEST_F(DecryptingVideoDecoderTest, Stop_AfterStop) {
  Initialize();
  EnterNormalDecodingState();
  Stop();
  Stop();
}

}  // namespace media

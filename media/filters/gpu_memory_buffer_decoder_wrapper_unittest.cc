// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gpu_memory_buffer_decoder_wrapper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/video/mock_gpu_memory_buffer_video_frame_pool.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace media {

class GpuMemoryBufferDecoderWrapperTest : public testing::Test {
 public:
  GpuMemoryBufferDecoderWrapperTest()
      : config_(TestVideoConfig::Normal()),
        decoder_(new testing::StrictMock<MockVideoDecoder>()),
        mock_pool_(new MockGpuMemoryBufferVideoFramePool(&frame_ready_cbs_)) {
    gmb_decoder_ = std::make_unique<GpuMemoryBufferDecoderWrapper>(
        std::unique_ptr<MockGpuMemoryBufferVideoFramePool>(mock_pool_),
        std::unique_ptr<VideoDecoder>(decoder_));
  }

  void Initialize(VideoDecoder::OutputCB* replaced_output_cb) {
    base::RunLoop loop;

    // Verify the InitCB given is actually called.
    EXPECT_CALL(*this, OnInitDone(true))
        .WillOnce(RunClosure(loop.QuitClosure()));

    VideoDecoder::WaitingForDecryptionKeyCB unused_cb;
    VideoDecoderConfig actual_config;
    EXPECT_CALL(*decoder_, Initialize(_, true, nullptr, _, _, _))
        .WillOnce(DoAll(SaveArg<0>(&actual_config),
                        SaveArg<4>(replaced_output_cb), RunCallback<3>(true),
                        SaveArg<5>(&unused_cb)));

    VideoDecoder::OutputCB output_cb =
        base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnOutputReady,
                            base::Unretained(this));
    gmb_decoder_->Initialize(
        config_, true, nullptr,
        base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnInitDone,
                            base::Unretained(this)),
        output_cb, base::DoNothing());
    loop.Run();

    // Verify the VideoDecoderConfig is passed correctly.
    ASSERT_TRUE(actual_config.Matches(config_));

    // Verify the OutputCB has been replaced with a custom one.
    ASSERT_FALSE(replaced_output_cb->Equals(output_cb));

    ASSERT_TRUE(unused_cb);
  }

  // Verifies EOS waits for all pending copies and returns the correct final
  // status in case of any underlying decoder errors.
  void TestDecodeEOSPendingCopies(DecodeStatus final_status) {
    VideoDecoder::OutputCB output_cb;
    Initialize(&output_cb);

    // The tests below queue multiple decodes.
    EXPECT_CALL(*decoder_, GetMaxDecodeRequests()).WillRepeatedly(Return(2));

    scoped_refptr<VideoFrame> frame_1 =
        VideoFrame::CreateBlackFrame(config_.coded_size());
    scoped_refptr<VideoFrame> frame_2 =
        VideoFrame::CreateBlackFrame(config_.coded_size());
    scoped_refptr<VideoFrame> frame_3 =
        VideoFrame::CreateBlackFrame(config_.coded_size());

    // Decode 1 buffer before the EOS, leave it pending for copying.
    {
      base::RunLoop loop;
      EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::OK))
          .WillOnce(RunClosure(loop.QuitClosure()));
      EXPECT_CALL(*decoder_, Decode(_, _))
          .WillOnce(DoAll(RunClosure(base::BindRepeating(output_cb, frame_1)),
                          RunCallback<1>(DecodeStatus::OK)));
      gmb_decoder_->Decode(
          base::MakeRefCounted<DecoderBuffer>(0),
          base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                              base::Unretained(this)));
      loop.Run();
      ASSERT_EQ(1u, frame_ready_cbs_.size());
    }

    // Decode EOS and output two more frames which should be pending for copies.
    {
      base::RunLoop loop;
      EXPECT_CALL(*decoder_, Decode(_, _))
          .WillOnce(
              DoAll(RunClosure(base::BindRepeating(output_cb, frame_2)),
                    RunClosure(base::BindRepeating(output_cb, frame_3)),
                    RunCallback<1>(final_status),
                    // Since OnDecodeDone() shouldn't be called until the frames
                    // finish copying, we need to quit the loop here.
                    RunClosure(loop.QuitClosure())));
      gmb_decoder_->Decode(
          DecoderBuffer::CreateEOSBuffer(),
          base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                              base::Unretained(this)));
      loop.Run();
      ASSERT_EQ(3u, frame_ready_cbs_.size());
    }

    // The following callbacks have a defined order (EOS DecodeCB must be run
    // only after all outputs), so ensure it.
    testing::InSequence verify_order;

    // Verify the first frame comes out okay.
    EXPECT_CALL(*this, OnOutputReady(frame_1));
    std::move(frame_ready_cbs_[0]).Run();

    // Next frame...
    EXPECT_CALL(*this, OnOutputReady(frame_2));
    std::move(frame_ready_cbs_[1]).Run();

    // And the final frame should also trigger the EOS decode CB.
    EXPECT_CALL(*this, OnOutputReady(frame_3));
    EXPECT_CALL(*this, OnDecodeDone(final_status));
    std::move(frame_ready_cbs_[2]).Run();
  }

  MOCK_METHOD1(OnInitDone, void(bool));
  MOCK_METHOD1(OnDecodeDone, void(DecodeStatus));
  MOCK_METHOD1(OnOutputReady, void(const scoped_refptr<VideoFrame>&));
  MOCK_METHOD0(OnResetDone, void(void));

  const VideoDecoderConfig config_;

  base::TestMessageLoop message_loop_;
  std::vector<base::OnceClosure> frame_ready_cbs_;

  std::unique_ptr<GpuMemoryBufferDecoderWrapper> gmb_decoder_;

  // Owned by |gmb_decoder_|.
  testing::StrictMock<MockVideoDecoder>* decoder_;
  MockGpuMemoryBufferVideoFramePool* mock_pool_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferDecoderWrapperTest);
};

// Verifies static methods (things safe to call from any thread) return the
// correct values.
TEST_F(GpuMemoryBufferDecoderWrapperTest, StaticMethods) {
  EXPECT_CALL(*decoder_, GetMaxDecodeRequests()).WillOnce(Return(2));
  ASSERT_EQ(2, gmb_decoder_->GetMaxDecodeRequests());
  ASSERT_EQ(decoder_->GetDisplayName(), gmb_decoder_->GetDisplayName());
}

// Verifies Initialize() and Reset() work properly. Verifies that Initialize()
// can be called after Reset().
TEST_F(GpuMemoryBufferDecoderWrapperTest, InitializeReset) {
  VideoDecoder::OutputCB output_cb;
  Initialize(&output_cb);
  EXPECT_TRUE(output_cb);

  EXPECT_CALL(*this, OnResetDone());
  EXPECT_CALL(*decoder_, Reset(_)).WillOnce(RunCallback<0>());
  EXPECT_CALL(*mock_pool_, Abort());
  gmb_decoder_->Reset(base::BindRepeating(
      &GpuMemoryBufferDecoderWrapperTest::OnResetDone, base::Unretained(this)));

  // Initialize should be okay to call again after Reset().
  Initialize(&output_cb);
  EXPECT_TRUE(output_cb);
}

// Verifies Initialize() can be called after EOS and Decode() works after.
TEST_F(GpuMemoryBufferDecoderWrapperTest, InitializeAfterEOS) {
  VideoDecoder::OutputCB output_cb;
  Initialize(&output_cb);
  EXPECT_TRUE(output_cb);

  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(config_.coded_size());

  {
    base::RunLoop loop;
    EXPECT_CALL(*decoder_, GetMaxDecodeRequests()).WillRepeatedly(Return(1));
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillOnce(DoAll(RunClosure(base::BindRepeating(output_cb, frame)),
                        RunCallback<1>(DecodeStatus::OK),
                        RunClosure(loop.QuitClosure())));
    gmb_decoder_->Decode(
        DecoderBuffer::CreateEOSBuffer(),
        base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                            base::Unretained(this)));
    loop.Run();
    ASSERT_EQ(1u, frame_ready_cbs_.size());
  }

  // Verify the pool returns the frame correctly and notifies EOS.
  EXPECT_CALL(*this, OnOutputReady(frame));
  EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::OK));
  std::move(frame_ready_cbs_.front()).Run();
  frame_ready_cbs_.clear();

  // Now we should be able to intialize and decode again.
  Initialize(&output_cb);

  {
    base::RunLoop loop;
    EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::OK))
        .WillOnce(RunClosure(loop.QuitClosure()));
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillOnce(DoAll(RunClosure(base::BindRepeating(output_cb, frame)),
                        RunCallback<1>(DecodeStatus::OK)));
    gmb_decoder_->Decode(
        base::MakeRefCounted<DecoderBuffer>(0),
        base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                            base::Unretained(this)));
    loop.Run();
    ASSERT_EQ(1u, frame_ready_cbs_.size());
  }

  // Verify the pool returns the frame correctly back to us.
  EXPECT_CALL(*this, OnOutputReady(frame));
  std::move(frame_ready_cbs_.front()).Run();
}

// Verify Decode() and OutputCB function as expected.
TEST_F(GpuMemoryBufferDecoderWrapperTest, Decode) {
  VideoDecoder::OutputCB output_cb;
  Initialize(&output_cb);

  base::RunLoop loop;

  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(config_.coded_size());

  EXPECT_CALL(*decoder_, GetMaxDecodeRequests()).WillRepeatedly(Return(1));
  EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::OK))
      .WillOnce(RunClosure(loop.QuitClosure()));
  EXPECT_CALL(*decoder_, Decode(_, _))
      .WillOnce(DoAll(RunClosure(base::BindRepeating(output_cb, frame)),
                      RunCallback<1>(DecodeStatus::OK)));
  gmb_decoder_->Decode(
      base::MakeRefCounted<DecoderBuffer>(0),
      base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                          base::Unretained(this)));
  loop.Run();
  ASSERT_EQ(1u, frame_ready_cbs_.size());

  // Verify the pool returns the frame correctly back to us.
  EXPECT_CALL(*this, OnOutputReady(frame));
  std::move(frame_ready_cbs_.front()).Run();
}

// Verify Decode() is limited by number of outstanding copies.
TEST_F(GpuMemoryBufferDecoderWrapperTest, DecodeIsLimitedByCopies) {
  VideoDecoder::OutputCB output_cb;
  Initialize(&output_cb);

  base::RunLoop loop;

  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(config_.coded_size());

  EXPECT_CALL(*decoder_, GetMaxDecodeRequests()).WillRepeatedly(Return(1));
  EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::OK))
      .WillOnce(RunClosure(loop.QuitClosure()));
  EXPECT_CALL(*decoder_, Decode(_, _))
      .WillOnce(DoAll(RunClosure(base::BindRepeating(output_cb, frame)),
                      RunClosure(base::BindRepeating(output_cb, frame)),
                      RunCallback<1>(DecodeStatus::OK)));
  gmb_decoder_->Decode(
      base::MakeRefCounted<DecoderBuffer>(0),
      base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                          base::Unretained(this)));
  loop.Run();
  ASSERT_EQ(2u, frame_ready_cbs_.size());

  // This decode should not run since there are 2 pending copies.
  gmb_decoder_->Decode(
      base::MakeRefCounted<DecoderBuffer>(0),
      base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                          base::Unretained(this)));

  // Verify the pool returns the frame correctly back to us. This should also
  // start the next Decode().
  EXPECT_CALL(*this, OnOutputReady(frame));
  EXPECT_CALL(*decoder_, Decode(_, _));
  std::move(frame_ready_cbs_.front()).Run();

  // Reset to ensure we don't leave callbacks uncalled during destruction.
  EXPECT_CALL(*mock_pool_, Abort());
  EXPECT_CALL(*decoder_, Reset(_));
  EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::ABORTED));
  gmb_decoder_->Reset(base::DoNothing());
}

// Verify Reset() aborts copies.
TEST_F(GpuMemoryBufferDecoderWrapperTest, ResetAbortsCopies) {
  VideoDecoder::OutputCB output_cb;
  Initialize(&output_cb);
  EXPECT_CALL(*this, OnOutputReady(_)).Times(0);
  EXPECT_CALL(*decoder_, GetMaxDecodeRequests()).WillRepeatedly(Return(1));

  {
    base::RunLoop loop;
    EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::OK))
        .WillOnce(RunClosure(loop.QuitClosure()));
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillOnce(DoAll(RunCallback<1>(DecodeStatus::OK),
                        // Issue output after the decode CB quits the loop, it
                        // should not run until Reset() starts the loop below.
                        RunClosure(BindToCurrentLoop(
                            base::BindRepeating(output_cb, nullptr)))));
    gmb_decoder_->Decode(
        base::MakeRefCounted<DecoderBuffer>(0),
        base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                            base::Unretained(this)));
    loop.Run();
    ASSERT_TRUE(frame_ready_cbs_.empty());
  }

  {
    base::RunLoop loop;
    // Use BindToCurrentLoop() to post the reset completion callback, so it will
    // run after anything that might currently be on the task runner queue.
    EXPECT_CALL(*this, OnResetDone()).WillOnce(RunClosure(loop.QuitClosure()));
    EXPECT_CALL(*decoder_, Reset(_)).WillOnce(RunCallback<0>());
    EXPECT_CALL(*mock_pool_, Abort());
    gmb_decoder_->Reset(BindToCurrentLoop(
        base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnResetDone,
                            base::Unretained(this))));
    loop.Run();
    ASSERT_TRUE(frame_ready_cbs_.empty());
  }

  testing::Mock::VerifyAndClearExpectations(this);

  // Verify a subsequent Decode() still functions correctly.
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(config_.coded_size());

  {
    base::RunLoop loop;
    EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::OK))
        .WillOnce(RunClosure(loop.QuitClosure()));

    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillOnce(DoAll(RunClosure(base::BindRepeating(output_cb, frame)),
                        RunCallback<1>(DecodeStatus::OK)));
    gmb_decoder_->Decode(
        base::MakeRefCounted<DecoderBuffer>(0),
        base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                            base::Unretained(this)));
    loop.Run();
    ASSERT_EQ(1u, frame_ready_cbs_.size());
  }

  // Verify the pool returns the frame correctly back to us.
  EXPECT_CALL(*this, OnOutputReady(frame));
  std::move(frame_ready_cbs_.front()).Run();
}

// Verify Reset() aborts an end of stream w/ pending copies where DecodeCB
// returns before the Reset() call.
TEST_F(GpuMemoryBufferDecoderWrapperTest,
       ResetAbortsEOSPendingCopiesAfterDecodeCBReturns) {
  VideoDecoder::OutputCB output_cb;
  Initialize(&output_cb);
  EXPECT_CALL(*this, OnOutputReady(_)).Times(0);
  EXPECT_CALL(*decoder_, GetMaxDecodeRequests()).WillRepeatedly(Return(1));

  scoped_refptr<VideoFrame> frame_1 =
      VideoFrame::CreateBlackFrame(config_.coded_size());

  // Decode 1 buffer before the EOS, leave it pending for copying.
  {
    base::RunLoop loop;
    EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::OK))
        .WillOnce(RunClosure(loop.QuitClosure()));
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillOnce(DoAll(RunClosure(base::BindRepeating(output_cb, frame_1)),
                        RunCallback<1>(DecodeStatus::OK)));
    gmb_decoder_->Decode(
        base::MakeRefCounted<DecoderBuffer>(0),
        base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                            base::Unretained(this)));
    loop.Run();
    ASSERT_EQ(1u, frame_ready_cbs_.size());
  }

  // The following calls should happen in order. DecodeDone -> Reset ->
  // ResetDone.
  testing::InSequence verify_order;

  base::RunLoop loop;
  EXPECT_CALL(*decoder_, Decode(_, _));
  gmb_decoder_->Decode(
      DecoderBuffer::CreateEOSBuffer(),
      base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                          base::Unretained(this)));

  EXPECT_CALL(*mock_pool_, Abort());
  EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::ABORTED));
  EXPECT_CALL(*decoder_, Reset(_)).WillOnce(RunCallback<0>());
  EXPECT_CALL(*this, OnResetDone()).WillOnce(RunClosure(loop.QuitClosure()));

  // Issue Reset() after the decode completes, this requires the Reset() call
  // to invoke the DecodeCB itself since no later copy or output can.
  gmb_decoder_->Reset(base::BindRepeating(
      &GpuMemoryBufferDecoderWrapperTest::OnResetDone, base::Unretained(this)));

  loop.Run();

  // Running our cached callback should do nothing.
  std::move(frame_ready_cbs_.front()).Run();
}

// Verify Reset() aborts an end of stream w/ pending copies. Similar to the
// above but with the DecodeCB queued after the Reset().
TEST_F(GpuMemoryBufferDecoderWrapperTest,
       ResetAbortsEOSPendingCopiesBeforeDecodeCBReturns) {
  VideoDecoder::OutputCB output_cb;
  Initialize(&output_cb);
  EXPECT_CALL(*this, OnOutputReady(_)).Times(0);
  EXPECT_CALL(*decoder_, GetMaxDecodeRequests()).WillRepeatedly(Return(2));

  scoped_refptr<VideoFrame> frame_1 =
      VideoFrame::CreateBlackFrame(config_.coded_size());

  // Decode 1 buffer before the EOS, leave it pending for copying.
  {
    base::RunLoop loop;
    EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::OK))
        .WillOnce(RunClosure(loop.QuitClosure()));
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillOnce(DoAll(RunClosure(base::BindRepeating(output_cb, frame_1)),
                        RunCallback<1>(DecodeStatus::OK)));
    gmb_decoder_->Decode(
        base::MakeRefCounted<DecoderBuffer>(0),
        base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                            base::Unretained(this)));
    loop.Run();
    ASSERT_EQ(1u, frame_ready_cbs_.size());
  }

  base::RunLoop loop;
  testing::InSequence verify_order;

  // Save DecodeCB and queue it after Reset().
  VideoDecoder::DecodeCB decode_cb;
  EXPECT_CALL(*decoder_, Decode(_, _)).WillOnce(SaveArg<1>(&decode_cb));
  gmb_decoder_->Decode(
      DecoderBuffer::CreateEOSBuffer(),
      base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                          base::Unretained(this)));

  EXPECT_CALL(*mock_pool_, Abort());
  EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::ABORTED));
  EXPECT_CALL(*decoder_, Reset(_))
      .WillOnce(
          DoAll(RunClosure(base::BindRepeating(decode_cb, DecodeStatus::OK)),
                RunCallback<0>()));
  EXPECT_CALL(*this, OnResetDone()).WillOnce(RunClosure(loop.QuitClosure()));

  // Issue Reset() before that decode can complete.
  gmb_decoder_->Reset(base::BindRepeating(
      &GpuMemoryBufferDecoderWrapperTest::OnResetDone, base::Unretained(this)));

  loop.Run();

  // Running our cached callback should do nothing.
  std::move(frame_ready_cbs_.front()).Run();
}

// Verify Decode() and OutputCB function as expected when an EOS is received and
// there are no pending copies.
TEST_F(GpuMemoryBufferDecoderWrapperTest, DecodeEOSNoPendingCopies) {
  VideoDecoder::OutputCB output_cb;
  Initialize(&output_cb);

  base::RunLoop loop;
  EXPECT_CALL(*decoder_, GetMaxDecodeRequests()).WillRepeatedly(Return(1));
  EXPECT_CALL(*this, OnDecodeDone(DecodeStatus::OK))
      .WillOnce(RunClosure(loop.QuitClosure()));
  EXPECT_CALL(*decoder_, Decode(_, _))
      .WillOnce(RunCallback<1>(DecodeStatus::OK));
  gmb_decoder_->Decode(
      DecoderBuffer::CreateEOSBuffer(),
      base::BindRepeating(&GpuMemoryBufferDecoderWrapperTest::OnDecodeDone,
                          base::Unretained(this)));
  loop.Run();
  ASSERT_TRUE(frame_ready_cbs_.empty());
}

// Verify Decode() and OutputCB function as expected when an EOS is received and
// there are pending copies.
TEST_F(GpuMemoryBufferDecoderWrapperTest, DecodeEOSPendingCopies) {
  TestDecodeEOSPendingCopies(DecodeStatus::OK);
}

// Verify Decode() and OutputCB function as expected when an EOS is received and
// there are pending copies and an error occurs.
TEST_F(GpuMemoryBufferDecoderWrapperTest, DecodeEOSPendingCopiesWithError) {
  TestDecodeEOSPendingCopies(DecodeStatus::DECODE_ERROR);
}

}  // namespace media

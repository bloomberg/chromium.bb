// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/debug/stack_trace.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/timer/timer.h"
#include "media/base/data_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/limits.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/filters/video_renderer_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

MATCHER_P(HasTimestamp, ms, "") {
  *result_listener << "has timestamp " << arg->timestamp().InMilliseconds();
  return arg->timestamp().InMilliseconds() == ms;
}

// Arbitrary value. Has to be larger to cover any timestamp value used in tests.
static const int kVideoDurationInMs = 1000;

class VideoRendererImplTest : public ::testing::Test {
 public:
  VideoRendererImplTest()
      : decoder_(new MockVideoDecoder()),
        demuxer_stream_(DemuxerStream::VIDEO) {
    ScopedVector<VideoDecoder> decoders;
    decoders.push_back(decoder_);

    renderer_.reset(new VideoRendererImpl(
        message_loop_.message_loop_proxy(),
        decoders.Pass(),
        media::SetDecryptorReadyCB(),
        base::Bind(&StrictMock<MockCB>::Display, base::Unretained(&mock_cb_)),
        true));

    demuxer_stream_.set_video_decoder_config(TestVideoConfig::Normal());

    // We expect these to be called but we don't care how/when.
    EXPECT_CALL(demuxer_stream_, Read(_)).WillRepeatedly(
        RunCallback<0>(DemuxerStream::kOk,
                       scoped_refptr<DecoderBuffer>(new DecoderBuffer(0))));
    EXPECT_CALL(statistics_cb_object_, OnStatistics(_))
        .Times(AnyNumber());
    EXPECT_CALL(*this, OnTimeUpdate(_))
        .Times(AnyNumber());
  }

  virtual ~VideoRendererImplTest() {}

  // Callbacks passed into Initialize().
  MOCK_METHOD1(OnTimeUpdate, void(base::TimeDelta));

  void Initialize() {
    InitializeWithLowDelay(false);
  }

  void InitializeWithLowDelay(bool low_delay) {
    // Monitor decodes from the decoder.
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillRepeatedly(Invoke(this, &VideoRendererImplTest::DecodeRequested));

    EXPECT_CALL(*decoder_, Reset(_))
        .WillRepeatedly(Invoke(this, &VideoRendererImplTest::FlushRequested));

    // Initialize, we shouldn't have any reads.
    InitializeRenderer(PIPELINE_OK, low_delay);
  }

  void InitializeRenderer(PipelineStatus expected, bool low_delay) {
    SCOPED_TRACE(base::StringPrintf("InitializeRenderer(%d)", expected));
    WaitableMessageLoopEvent event;
    CallInitialize(event.GetPipelineStatusCB(), low_delay, expected);
    event.RunAndWaitForStatus(expected);
  }

  void CallInitialize(const PipelineStatusCB& status_cb,
                      bool low_delay,
                      PipelineStatus decoder_status) {
    EXPECT_CALL(*decoder_, Initialize(_, _, _, _)).WillOnce(
        DoAll(SaveArg<3>(&output_cb_), RunCallback<2>(decoder_status)));
    renderer_->Initialize(
        &demuxer_stream_,
        low_delay,
        status_cb,
        base::Bind(&MockStatisticsCB::OnStatistics,
                   base::Unretained(&statistics_cb_object_)),
        base::Bind(&VideoRendererImplTest::OnTimeUpdate,
                   base::Unretained(this)),
        base::Bind(&StrictMock<MockCB>::BufferingStateChange,
                   base::Unretained(&mock_cb_)),
        ended_event_.GetClosure(),
        error_event_.GetPipelineStatusCB(),
        base::Bind(&VideoRendererImplTest::GetTime, base::Unretained(this)),
        base::Bind(&VideoRendererImplTest::GetDuration,
                   base::Unretained(this)));
  }

  void StartPlaying() {
    SCOPED_TRACE("StartPlaying()");
    renderer_->StartPlaying();
    message_loop_.RunUntilIdle();
  }

  void Flush() {
    SCOPED_TRACE("Flush()");
    WaitableMessageLoopEvent event;
    renderer_->Flush(event.GetClosure());
    event.RunAndWait();
  }

  void Destroy() {
    SCOPED_TRACE("Destroy()");
    renderer_.reset();
  }

  // Parses a string representation of video frames and generates corresponding
  // VideoFrame objects in |decode_results_|.
  //
  // Syntax:
  //   nn - Queue a decoder buffer with timestamp nn * 1000us
  //   abort - Queue an aborted read
  //   error - Queue a decoder error
  //
  // Examples:
  //   A clip that is four frames long: "0 10 20 30"
  //   A clip that has a decode error: "60 70 error"
  void QueueFrames(const std::string& str) {
    std::vector<std::string> tokens;
    base::SplitString(str, ' ', &tokens);
    for (size_t i = 0; i < tokens.size(); ++i) {
      if (tokens[i] == "abort") {
        scoped_refptr<VideoFrame> null_frame;
        decode_results_.push_back(
            std::make_pair(VideoDecoder::kAborted, null_frame));
        continue;
      }

      if (tokens[i] == "error") {
        scoped_refptr<VideoFrame> null_frame;
        decode_results_.push_back(
            std::make_pair(VideoDecoder::kDecodeError, null_frame));
        continue;
      }

      int timestamp_in_ms = 0;
      if (base::StringToInt(tokens[i], &timestamp_in_ms)) {
        gfx::Size natural_size = TestVideoConfig::NormalCodedSize();
        scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
            VideoFrame::YV12,
            natural_size,
            gfx::Rect(natural_size),
            natural_size,
            base::TimeDelta::FromMilliseconds(timestamp_in_ms));
        decode_results_.push_back(std::make_pair(VideoDecoder::kOk, frame));
        continue;
      }

      CHECK(false) << "Unrecognized decoder buffer token: " << tokens[i];
    }
  }

  bool IsReadPending() {
    return !decode_cb_.is_null();
  }

  void WaitForError(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("WaitForError(%d)", expected));
    error_event_.RunAndWaitForStatus(expected);
  }

  void WaitForEnded() {
    SCOPED_TRACE("WaitForEnded()");
    ended_event_.RunAndWait();
  }

  void WaitForPendingRead() {
    SCOPED_TRACE("WaitForPendingRead()");
    if (!decode_cb_.is_null())
      return;

    DCHECK(wait_for_pending_decode_cb_.is_null());

    WaitableMessageLoopEvent event;
    wait_for_pending_decode_cb_ = event.GetClosure();
    event.RunAndWait();

    DCHECK(!decode_cb_.is_null());
    DCHECK(wait_for_pending_decode_cb_.is_null());
  }

  void SatisfyPendingRead() {
    CHECK(!decode_cb_.is_null());
    CHECK(!decode_results_.empty());

    // Post tasks for OutputCB and DecodeCB.
    scoped_refptr<VideoFrame> frame = decode_results_.front().second;
    if (frame)
      message_loop_.PostTask(FROM_HERE, base::Bind(output_cb_, frame));
    message_loop_.PostTask(
        FROM_HERE, base::Bind(base::ResetAndReturn(&decode_cb_),
                              decode_results_.front().first));
    decode_results_.pop_front();
  }

  void SatisfyPendingReadWithEndOfStream() {
    DCHECK(!decode_cb_.is_null());

    // Return EOS buffer to trigger EOS frame.
    EXPECT_CALL(demuxer_stream_, Read(_))
        .WillOnce(RunCallback<0>(DemuxerStream::kOk,
                                 DecoderBuffer::CreateEOSBuffer()));

    // Satify pending |decode_cb_| to trigger a new DemuxerStream::Read().
    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(base::ResetAndReturn(&decode_cb_), VideoDecoder::kOk));

    WaitForPendingRead();

    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(base::ResetAndReturn(&decode_cb_), VideoDecoder::kOk));
  }

  void AdvanceTimeInMs(int time_ms) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    base::AutoLock l(lock_);
    time_ += base::TimeDelta::FromMilliseconds(time_ms);
    DCHECK_LE(time_.InMicroseconds(), GetDuration().InMicroseconds());
  }

 protected:
  // Fixture members.
  scoped_ptr<VideoRendererImpl> renderer_;
  MockVideoDecoder* decoder_;  // Owned by |renderer_|.
  NiceMock<MockDemuxerStream> demuxer_stream_;
  MockStatisticsCB statistics_cb_object_;

  // Use StrictMock<T> to catch missing/extra callbacks.
  class MockCB {
   public:
    MOCK_METHOD1(Display, void(const scoped_refptr<VideoFrame>&));
    MOCK_METHOD1(BufferingStateChange, void(BufferingState));
  };
  StrictMock<MockCB> mock_cb_;

 private:
  base::TimeDelta GetTime() {
    base::AutoLock l(lock_);
    return time_;
  }

  base::TimeDelta GetDuration() {
    return base::TimeDelta::FromMilliseconds(kVideoDurationInMs);
  }

  void DecodeRequested(const scoped_refptr<DecoderBuffer>& buffer,
                       const VideoDecoder::DecodeCB& decode_cb) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    CHECK(decode_cb_.is_null());
    decode_cb_ = decode_cb;

    // Wake up WaitForPendingRead() if needed.
    if (!wait_for_pending_decode_cb_.is_null())
      base::ResetAndReturn(&wait_for_pending_decode_cb_).Run();

    if (decode_results_.empty())
      return;

    SatisfyPendingRead();
  }

  void FlushRequested(const base::Closure& callback) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    decode_results_.clear();
    if (!decode_cb_.is_null()) {
      QueueFrames("abort");
      SatisfyPendingRead();
    }

    message_loop_.PostTask(FROM_HERE, callback);
  }

  base::MessageLoop message_loop_;

  // Used to protect |time_|.
  base::Lock lock_;
  base::TimeDelta time_;

  // Used for satisfying reads.
  VideoDecoder::OutputCB output_cb_;
  VideoDecoder::DecodeCB decode_cb_;
  base::TimeDelta next_frame_timestamp_;

  WaitableMessageLoopEvent error_event_;
  WaitableMessageLoopEvent ended_event_;

  // Run during DecodeRequested() to unblock WaitForPendingRead().
  base::Closure wait_for_pending_decode_cb_;

  std::deque<std::pair<
      VideoDecoder::Status, scoped_refptr<VideoFrame> > > decode_results_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererImplTest);
};

TEST_F(VideoRendererImplTest, DoNothing) {
  // Test that creation and deletion doesn't depend on calls to Initialize()
  // and/or Destroy().
}

TEST_F(VideoRendererImplTest, DestroyWithoutInitialize) {
  Destroy();
}

TEST_F(VideoRendererImplTest, Initialize) {
  Initialize();
  Destroy();
}

TEST_F(VideoRendererImplTest, InitializeAndStartPlaying) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, Display(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlaying();
  Destroy();
}

TEST_F(VideoRendererImplTest, DestroyWhileInitializing) {
  CallInitialize(NewExpectedStatusCB(PIPELINE_ERROR_ABORT), false, PIPELINE_OK);
  Destroy();
}

TEST_F(VideoRendererImplTest, DestroyWhileFlushing) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, Display(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlaying();
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_NOTHING));
  renderer_->Flush(NewExpectedClosure());
  Destroy();
}

TEST_F(VideoRendererImplTest, Play) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, Display(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlaying();
  Destroy();
}

TEST_F(VideoRendererImplTest, FlushWithNothingBuffered) {
  Initialize();
  StartPlaying();

  // We shouldn't expect a buffering state change since we never reached
  // BUFFERING_HAVE_ENOUGH.
  Flush();
  Destroy();
}

TEST_F(VideoRendererImplTest, EndOfStream_ClipDuration) {
  Initialize();
  QueueFrames("0");
  EXPECT_CALL(mock_cb_, Display(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlaying();

  // Next frame has timestamp way past duration. Its timestamp will be adjusted
  // to match the duration of the video.
  QueueFrames(base::IntToString(kVideoDurationInMs + 1000));
  SatisfyPendingRead();
  WaitForPendingRead();

  // Queue the end of stream frame and wait for the last frame to be rendered.
  SatisfyPendingReadWithEndOfStream();

  EXPECT_CALL(mock_cb_, Display(HasTimestamp(kVideoDurationInMs)));
  AdvanceTimeInMs(kVideoDurationInMs);
  WaitForEnded();

  Destroy();
}

TEST_F(VideoRendererImplTest, DecodeError_Playing) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, Display(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlaying();

  QueueFrames("error");
  SatisfyPendingRead();
  WaitForError(PIPELINE_ERROR_DECODE);
  Destroy();
}

TEST_F(VideoRendererImplTest, DecodeError_DuringStartPlaying) {
  Initialize();
  QueueFrames("error");
  StartPlaying();
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlaying_Exact) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, Display(HasTimestamp(60)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  AdvanceTimeInMs(60);
  StartPlaying();
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlaying_RightBefore) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, Display(HasTimestamp(50)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  AdvanceTimeInMs(59);
  StartPlaying();
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlaying_RightAfter) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, Display(HasTimestamp(60)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  AdvanceTimeInMs(61);
  StartPlaying();
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlaying_LowDelay) {
  // In low-delay mode only one frame is required to finish preroll.
  InitializeWithLowDelay(true);
  QueueFrames("0");

  // Expect some amount of have enough/nothing due to only requiring one frame.
  EXPECT_CALL(mock_cb_, Display(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(AnyNumber());
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(AnyNumber());
  StartPlaying();

  QueueFrames("10");
  SatisfyPendingRead();

  WaitableMessageLoopEvent event;
  EXPECT_CALL(mock_cb_, Display(HasTimestamp(10)))
      .WillOnce(RunClosure(event.GetClosure()));
  AdvanceTimeInMs(10);
  event.RunAndWait();

  Destroy();
}

TEST_F(VideoRendererImplTest, PlayAfterStartPlaying) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, Display(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlaying();

  // Check that there is an outstanding Read() request.
  EXPECT_TRUE(IsReadPending());

  Destroy();
}

// Verify that a late decoder response doesn't break invariants in the renderer.
TEST_F(VideoRendererImplTest, DestroyDuringOutstandingRead) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, Display(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlaying();

  // Check that there is an outstanding Read() request.
  EXPECT_TRUE(IsReadPending());

  Destroy();
}

TEST_F(VideoRendererImplTest, VideoDecoder_InitFailure) {
  InitializeRenderer(DECODER_ERROR_NOT_SUPPORTED, false);
  Destroy();
}

TEST_F(VideoRendererImplTest, Underflow) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, Display(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH));
  StartPlaying();

  // Frames should be dropped and we should signal having nothing.
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_NOTHING");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_NOTHING))
        .WillOnce(RunClosure(event.GetClosure()));
    AdvanceTimeInMs(100);
    event.RunAndWait();
  }

  // Receiving end of stream should signal having enough.
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, BufferingStateChange(BUFFERING_HAVE_ENOUGH))
        .WillOnce(RunClosure(event.GetClosure()));
    SatisfyPendingReadWithEndOfStream();
    event.RunAndWait();
  }

  WaitForEnded();
  Destroy();
}

}  // namespace media

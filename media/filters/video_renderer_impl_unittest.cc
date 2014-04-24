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
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

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

    renderer_.reset(
        new VideoRendererImpl(message_loop_.message_loop_proxy(),
                              decoders.Pass(),
                              media::SetDecryptorReadyCB(),
                              base::Bind(&StrictMock<MockDisplayCB>::Display,
                                         base::Unretained(&mock_display_cb_)),
                              true));

    demuxer_stream_.set_video_decoder_config(TestVideoConfig::Normal());

    // We expect these to be called but we don't care how/when.
    EXPECT_CALL(demuxer_stream_, Read(_))
        .WillRepeatedly(RunCallback<0>(DemuxerStream::kOk,
                                       DecoderBuffer::CreateEOSBuffer()));
    EXPECT_CALL(*decoder_, Stop())
        .WillRepeatedly(Invoke(this, &VideoRendererImplTest::StopRequested));
    EXPECT_CALL(statistics_cb_object_, OnStatistics(_))
        .Times(AnyNumber());
    EXPECT_CALL(*this, OnTimeUpdate(_))
        .Times(AnyNumber());
  }

  virtual ~VideoRendererImplTest() {}

  // Callbacks passed into Initialize().
  MOCK_METHOD1(OnTimeUpdate, void(base::TimeDelta));

  void Initialize() {
    // Monitor decodes from the decoder.
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillRepeatedly(Invoke(this, &VideoRendererImplTest::FrameRequested));

    EXPECT_CALL(*decoder_, Reset(_))
        .WillRepeatedly(Invoke(this, &VideoRendererImplTest::FlushRequested));

    InSequence s;

    EXPECT_CALL(*decoder_, Initialize(_, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));

    // Set playback rate before anything else happens.
    renderer_->SetPlaybackRate(1.0f);

    // Initialize, we shouldn't have any reads.
    InitializeRenderer(PIPELINE_OK);
  }

  void InitializeRenderer(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("InitializeRenderer(%d)", expected));
    WaitableMessageLoopEvent event;
    CallInitialize(event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(expected);
  }

  void CallInitialize(const PipelineStatusCB& status_cb) {
    renderer_->Initialize(
        &demuxer_stream_,
        status_cb,
        base::Bind(&MockStatisticsCB::OnStatistics,
                   base::Unretained(&statistics_cb_object_)),
        base::Bind(&VideoRendererImplTest::OnTimeUpdate,
                   base::Unretained(this)),
        ended_event_.GetClosure(),
        error_event_.GetPipelineStatusCB(),
        base::Bind(&VideoRendererImplTest::GetTime, base::Unretained(this)),
        base::Bind(&VideoRendererImplTest::GetDuration,
                   base::Unretained(this)));
  }

  void Play() {
    SCOPED_TRACE("Play()");
    WaitableMessageLoopEvent event;
    renderer_->Play(event.GetClosure());
    event.RunAndWait();
  }

  void Preroll(int timestamp_ms, PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("Preroll(%d, %d)", timestamp_ms, expected));
    WaitableMessageLoopEvent event;
    renderer_->Preroll(
        base::TimeDelta::FromMilliseconds(timestamp_ms),
        event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(expected);
  }

  void Pause() {
    SCOPED_TRACE("Pause()");
    WaitableMessageLoopEvent event;
    renderer_->Pause(event.GetClosure());
    event.RunAndWait();
  }

  void Flush() {
    SCOPED_TRACE("Flush()");
    WaitableMessageLoopEvent event;
    renderer_->Flush(event.GetClosure());
    event.RunAndWait();
  }

  void Stop() {
    SCOPED_TRACE("Stop()");
    WaitableMessageLoopEvent event;
    renderer_->Stop(event.GetClosure());
    event.RunAndWait();
  }

  void Shutdown() {
    Pause();
    Flush();
    Stop();
  }

  // Parses a string representation of video frames and generates corresponding
  // VideoFrame objects in |decode_results_|.
  //
  // Syntax:
  //   nn - Queue a decoder buffer with timestamp nn * 1000us
  //   abort - Queue an aborted read
  //   error - Queue a decoder error
  //   eos - Queue an end of stream decoder buffer
  //
  // Examples:
  //   A clip that is four frames long: "0 10 20 30 eos"
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

      if (tokens[i] == "eos") {
        decode_results_.push_back(
            std::make_pair(VideoDecoder::kOk, VideoFrame::CreateEOSFrame()));
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
    if (!read_cb_.is_null())
      return;

    DCHECK(wait_for_pending_read_cb_.is_null());

    WaitableMessageLoopEvent event;
    wait_for_pending_read_cb_ = event.GetClosure();
    event.RunAndWait();

    DCHECK(!read_cb_.is_null());
    DCHECK(wait_for_pending_read_cb_.is_null());
  }

  void SatisfyPendingRead() {
    CHECK(!read_cb_.is_null());
    CHECK(!decode_results_.empty());

    base::Closure closure = base::Bind(
        read_cb_, decode_results_.front().first,
        decode_results_.front().second);

    read_cb_.Reset();
    decode_results_.pop_front();

    message_loop_.PostTask(FROM_HERE, closure);
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

  // Use StrictMock<T> to catch missing/extra display callbacks.
  class MockDisplayCB {
   public:
    MOCK_METHOD1(Display, void(const scoped_refptr<VideoFrame>&));
  };
  StrictMock<MockDisplayCB> mock_display_cb_;

 private:
  base::TimeDelta GetTime() {
    base::AutoLock l(lock_);
    return time_;
  }

  base::TimeDelta GetDuration() {
    return base::TimeDelta::FromMilliseconds(kVideoDurationInMs);
  }

  void FrameRequested(const scoped_refptr<DecoderBuffer>& buffer,
                      const VideoDecoder::DecodeCB& read_cb) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    CHECK(read_cb_.is_null());
    read_cb_ = read_cb;

    // Wake up WaitForPendingRead() if needed.
    if (!wait_for_pending_read_cb_.is_null())
      base::ResetAndReturn(&wait_for_pending_read_cb_).Run();

    if (decode_results_.empty())
      return;

    SatisfyPendingRead();
  }

  void FlushRequested(const base::Closure& callback) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    decode_results_.clear();
    if (!read_cb_.is_null()) {
      QueueFrames("abort");
      SatisfyPendingRead();
    }

    message_loop_.PostTask(FROM_HERE, callback);
  }

  void StopRequested() {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    decode_results_.clear();
    if (!read_cb_.is_null()) {
      QueueFrames("abort");
      SatisfyPendingRead();
    }
  }

  base::MessageLoop message_loop_;

  // Used to protect |time_|.
  base::Lock lock_;
  base::TimeDelta time_;

  // Used for satisfying reads.
  VideoDecoder::DecodeCB read_cb_;
  base::TimeDelta next_frame_timestamp_;

  WaitableMessageLoopEvent error_event_;
  WaitableMessageLoopEvent ended_event_;

  // Run during FrameRequested() to unblock WaitForPendingRead().
  base::Closure wait_for_pending_read_cb_;

  std::deque<std::pair<
      VideoDecoder::Status, scoped_refptr<VideoFrame> > > decode_results_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererImplTest);
};

TEST_F(VideoRendererImplTest, DoNothing) {
  // Test that creation and deletion doesn't depend on calls to Initialize()
  // and/or Stop().
}

TEST_F(VideoRendererImplTest, StopWithoutInitialize) {
  Stop();
}

TEST_F(VideoRendererImplTest, Initialize) {
  Initialize();
  Shutdown();
}

TEST_F(VideoRendererImplTest, InitializeAndPreroll) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);
  Shutdown();
}

static void ExpectNotCalled(PipelineStatus) {
  base::debug::StackTrace stack;
  ADD_FAILURE() << "Expected callback not to be called\n" << stack.ToString();
}

TEST_F(VideoRendererImplTest, StopWhileInitializing) {
  EXPECT_CALL(*decoder_, Initialize(_, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  CallInitialize(base::Bind(&ExpectNotCalled));
  Stop();

  // ~VideoRendererImpl() will CHECK() if we left anything initialized.
}

TEST_F(VideoRendererImplTest, StopWhileFlushing) {
  Initialize();
  Pause();
  renderer_->Flush(base::Bind(&ExpectNotCalled, PIPELINE_OK));
  Stop();

  // ~VideoRendererImpl() will CHECK() if we left anything initialized.
}

TEST_F(VideoRendererImplTest, Play) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);
  Play();
  Shutdown();
}

TEST_F(VideoRendererImplTest, EndOfStream_ClipDuration) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);
  Play();

  // Next frame has timestamp way past duration. Its timestamp will be adjusted
  // to match the duration of the video.
  QueueFrames(base::IntToString(kVideoDurationInMs + 1000));

  // Queue the end of stream frame and wait for the last frame to be rendered.
  QueueFrames("eos");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(kVideoDurationInMs)));
  AdvanceTimeInMs(kVideoDurationInMs);
  WaitForEnded();

  Shutdown();
}

TEST_F(VideoRendererImplTest, DecodeError_Playing) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);
  Play();

  QueueFrames("error");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(10)));
  AdvanceTimeInMs(10);
  WaitForError(PIPELINE_ERROR_DECODE);
  Shutdown();
}

TEST_F(VideoRendererImplTest, DecodeError_DuringPreroll) {
  Initialize();
  QueueFrames("error");
  Preroll(0, PIPELINE_ERROR_DECODE);
  Shutdown();
}

TEST_F(VideoRendererImplTest, Preroll_Exact) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(60)));
  Preroll(60, PIPELINE_OK);
  Shutdown();
}

TEST_F(VideoRendererImplTest, Preroll_RightBefore) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(50)));
  Preroll(59, PIPELINE_OK);
  Shutdown();
}

TEST_F(VideoRendererImplTest, Preroll_RightAfter) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(60)));
  Preroll(61, PIPELINE_OK);
  Shutdown();
}

TEST_F(VideoRendererImplTest, PlayAfterPreroll) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);
  Play();

  // Advance time past prerolled time to trigger a Read().
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(10)));
  AdvanceTimeInMs(10);
  WaitForPendingRead();
  Shutdown();
}

TEST_F(VideoRendererImplTest, Rebuffer) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);
  Play();

  // Advance time past prerolled time drain the ready frame queue.
  AdvanceTimeInMs(50);
  WaitForPendingRead();

  // Simulate a Pause/Preroll/Play rebuffer sequence.
  Pause();

  WaitableMessageLoopEvent event;
  renderer_->Preroll(kNoTimestamp(),
                     event.GetPipelineStatusCB());

  // Queue enough frames to satisfy preroll.
  QueueFrames("40 50 60 70");
  SatisfyPendingRead();

  // TODO(scherkus): We shouldn't display the next ready frame in a rebuffer
  // situation, see http://crbug.com/365516
  EXPECT_CALL(mock_display_cb_, Display(_));

  event.RunAndWaitForStatus(PIPELINE_OK);

  Play();

  Shutdown();
}

TEST_F(VideoRendererImplTest, Rebuffer_AlreadyHaveEnoughFrames) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);

  // Queue an extra frame so that we'll have enough frames to satisfy
  // preroll even after the first frame is painted.
  QueueFrames("40");
  Play();

  // Simulate a Pause/Preroll/Play rebuffer sequence.
  Pause();

  // TODO(scherkus): We shouldn't display the next ready frame in a rebuffer
  // situation, see http://crbug.com/365516
  EXPECT_CALL(mock_display_cb_, Display(_));

  WaitableMessageLoopEvent event;
  renderer_->Preroll(kNoTimestamp(),
                     event.GetPipelineStatusCB());

  event.RunAndWaitForStatus(PIPELINE_OK);

  Play();

  Shutdown();
}

// Verify that a late decoder response doesn't break invariants in the renderer.
TEST_F(VideoRendererImplTest, StopDuringOutstandingRead) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);
  Play();

  // Advance time a bit to trigger a Read().
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(10)));
  AdvanceTimeInMs(10);
  WaitForPendingRead();

  WaitableMessageLoopEvent event;
  renderer_->Stop(event.GetClosure());
  event.RunAndWait();
}

TEST_F(VideoRendererImplTest, AbortPendingRead_Playing) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);
  Play();

  // Advance time a bit to trigger a Read().
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(10)));
  AdvanceTimeInMs(10);
  WaitForPendingRead();
  QueueFrames("abort");
  SatisfyPendingRead();

  Pause();
  Flush();
  QueueFrames("60 70 80 90");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(60)));
  Preroll(60, PIPELINE_OK);
  Shutdown();
}

TEST_F(VideoRendererImplTest, AbortPendingRead_Flush) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);
  Play();

  // Advance time a bit to trigger a Read().
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(10)));
  AdvanceTimeInMs(10);
  WaitForPendingRead();

  Pause();
  Flush();
  Shutdown();
}

TEST_F(VideoRendererImplTest, AbortPendingRead_Preroll) {
  Initialize();
  QueueFrames("0 10 abort");
  EXPECT_CALL(mock_display_cb_, Display(HasTimestamp(0)));
  Preroll(0, PIPELINE_OK);
  Shutdown();
}

TEST_F(VideoRendererImplTest, VideoDecoder_InitFailure) {
  InSequence s;

  EXPECT_CALL(*decoder_, Initialize(_, _))
      .WillOnce(RunCallback<1>(DECODER_ERROR_NOT_SUPPORTED));
  InitializeRenderer(DECODER_ERROR_NOT_SUPPORTED);

  Stop();
}

}  // namespace media

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

static const int kFrameDurationInMs = 10;
static const int kVideoDurationInMs = kFrameDurationInMs * 100;

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
        base::Bind(&VideoRendererImplTest::OnPaint, base::Unretained(this)),
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
    InitializeWithDuration(kVideoDurationInMs);
  }

  void InitializeWithDuration(int duration_ms) {
    duration_ = base::TimeDelta::FromMilliseconds(duration_ms);

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

    // Start prerolling.
    QueuePrerollFrames(0);
    Preroll(0, PIPELINE_OK);
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

  // Queues a VideoFrame with |next_frame_timestamp_|.
  void QueueNextFrame() {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    DCHECK_LT(next_frame_timestamp_.InMicroseconds(),
              duration_.InMicroseconds());

    gfx::Size natural_size = TestVideoConfig::NormalCodedSize();
    scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
        VideoFrame::YV12, natural_size, gfx::Rect(natural_size), natural_size,
        next_frame_timestamp_);
    decode_results_.push_back(std::make_pair(
        VideoDecoder::kOk, frame));
    next_frame_timestamp_ +=
        base::TimeDelta::FromMilliseconds(kFrameDurationInMs);
  }

  void QueueEndOfStream() {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    decode_results_.push_back(std::make_pair(
        VideoDecoder::kOk, VideoFrame::CreateEOSFrame()));
  }

  void QueueDecodeError() {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    scoped_refptr<VideoFrame> null_frame;
    decode_results_.push_back(std::make_pair(
        VideoDecoder::kDecodeError, null_frame));
  }

  void QueueAbortedRead() {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    scoped_refptr<VideoFrame> null_frame;
    decode_results_.push_back(std::make_pair(
        VideoDecoder::kAborted, null_frame));
  }

  void QueuePrerollFrames(int timestamp_ms) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    next_frame_timestamp_ = base::TimeDelta();
    base::TimeDelta timestamp = base::TimeDelta::FromMilliseconds(timestamp_ms);
    while (next_frame_timestamp_ < timestamp) {
      QueueNextFrame();
    }

    // Queue the frame at |timestamp| plus additional ones for prerolling.
    for (int i = 0; i < limits::kMaxVideoFrames; ++i) {
      QueueNextFrame();
    }
  }

  void ResetCurrentFrame() {
    base::AutoLock l(lock_);
    current_frame_ = NULL;
  }

  scoped_refptr<VideoFrame> GetCurrentFrame() {
    base::AutoLock l(lock_);
    return current_frame_;
  }

  int GetCurrentTimestampInMs() {
    scoped_refptr<VideoFrame> frame = GetCurrentFrame();
    if (!frame.get())
      return -1;
    return frame->timestamp().InMilliseconds();
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
    DCHECK_LE(time_.InMicroseconds(), duration_.InMicroseconds());
  }

 protected:
  // Fixture members.
  scoped_ptr<VideoRendererImpl> renderer_;
  MockVideoDecoder* decoder_;  // Owned by |renderer_|.
  NiceMock<MockDemuxerStream> demuxer_stream_;
  MockStatisticsCB statistics_cb_object_;

 private:
  base::TimeDelta GetTime() {
    base::AutoLock l(lock_);
    return time_;
  }

  base::TimeDelta GetDuration() {
    return duration_;
  }

  void OnPaint(const scoped_refptr<VideoFrame>& frame) {
    base::AutoLock l(lock_);
    current_frame_ = frame;
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
      QueueAbortedRead();
      SatisfyPendingRead();
    }

    message_loop_.PostTask(FROM_HERE, callback);
  }

  void StopRequested() {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    decode_results_.clear();
    if (!read_cb_.is_null()) {
      QueueAbortedRead();
      SatisfyPendingRead();
    }
  }

  base::MessageLoop message_loop_;

  // Used to protect |time_| and |current_frame_|.
  base::Lock lock_;
  base::TimeDelta time_;
  scoped_refptr<VideoFrame> current_frame_;

  // Used for satisfying reads.
  VideoDecoder::DecodeCB read_cb_;
  base::TimeDelta next_frame_timestamp_;
  base::TimeDelta duration_;

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
  EXPECT_EQ(0, GetCurrentTimestampInMs());
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
  Play();
  Shutdown();
}

TEST_F(VideoRendererImplTest, EndOfStream_DefaultFrameDuration) {
  Initialize();
  Play();

  // Verify that the ended callback fires when the default last frame duration
  // has elapsed.
  int end_timestamp = kFrameDurationInMs * limits::kMaxVideoFrames +
      VideoRendererImpl::kMaxLastFrameDuration().InMilliseconds();
  EXPECT_LT(end_timestamp, kVideoDurationInMs);

  QueueEndOfStream();
  AdvanceTimeInMs(end_timestamp);
  WaitForEnded();

  Shutdown();
}

TEST_F(VideoRendererImplTest, EndOfStream_ClipDuration) {
  int duration = kVideoDurationInMs + kFrameDurationInMs / 2;
  InitializeWithDuration(duration);
  Play();

  // Render all frames except for the last |limits::kMaxVideoFrames| frames
  // and deliver all the frames between the start and |duration|. The preroll
  // inside Initialize() makes this a little confusing, but |timestamp| is
  // the current render time and QueueNextFrame() delivers a frame with a
  // timestamp that is |timestamp| + limits::kMaxVideoFrames *
  // kFrameDurationInMs.
  int timestamp = kFrameDurationInMs;
  int end_timestamp = duration - limits::kMaxVideoFrames * kFrameDurationInMs;
  for (; timestamp < end_timestamp; timestamp += kFrameDurationInMs) {
    QueueNextFrame();
  }

  // Queue the end of stream frame and wait for the last frame to be rendered.
  QueueEndOfStream();
  AdvanceTimeInMs(duration);
  WaitForEnded();

  Shutdown();
}

TEST_F(VideoRendererImplTest, DecodeError_Playing) {
  Initialize();
  Play();

  QueueDecodeError();
  AdvanceTimeInMs(kVideoDurationInMs);
  WaitForError(PIPELINE_ERROR_DECODE);
  Shutdown();
}

TEST_F(VideoRendererImplTest, DecodeError_DuringPreroll) {
  Initialize();
  Pause();
  Flush();

  QueueDecodeError();
  Preroll(kFrameDurationInMs * 6, PIPELINE_ERROR_DECODE);
  Shutdown();
}

TEST_F(VideoRendererImplTest, Preroll_Exact) {
  Initialize();
  Pause();
  Flush();
  QueuePrerollFrames(kFrameDurationInMs * 6);

  Preroll(kFrameDurationInMs * 6, PIPELINE_OK);
  EXPECT_EQ(kFrameDurationInMs * 6, GetCurrentTimestampInMs());
  Shutdown();
}

TEST_F(VideoRendererImplTest, Preroll_RightBefore) {
  Initialize();
  Pause();
  Flush();
  QueuePrerollFrames(kFrameDurationInMs * 6);

  Preroll(kFrameDurationInMs * 6 - 1, PIPELINE_OK);
  EXPECT_EQ(kFrameDurationInMs * 5, GetCurrentTimestampInMs());
  Shutdown();
}

TEST_F(VideoRendererImplTest, Preroll_RightAfter) {
  Initialize();
  Pause();
  Flush();
  QueuePrerollFrames(kFrameDurationInMs * 6);

  Preroll(kFrameDurationInMs * 6 + 1, PIPELINE_OK);
  EXPECT_EQ(kFrameDurationInMs * 6, GetCurrentTimestampInMs());
  Shutdown();
}

TEST_F(VideoRendererImplTest, PlayAfterPreroll) {
  Initialize();
  Pause();
  Flush();
  QueuePrerollFrames(kFrameDurationInMs * 4);

  Preroll(kFrameDurationInMs * 4, PIPELINE_OK);
  EXPECT_EQ(kFrameDurationInMs * 4, GetCurrentTimestampInMs());

  Play();
  // Advance time past prerolled time to trigger a Read().
  AdvanceTimeInMs(5 * kFrameDurationInMs);
  WaitForPendingRead();
  Shutdown();
}

TEST_F(VideoRendererImplTest, Rebuffer) {
  Initialize();

  Play();

  // Advance time past prerolled time drain the ready frame queue.
  AdvanceTimeInMs(5 * kFrameDurationInMs);
  WaitForPendingRead();

  // Simulate a Pause/Preroll/Play rebuffer sequence.
  Pause();

  WaitableMessageLoopEvent event;
  renderer_->Preroll(kNoTimestamp(),
                     event.GetPipelineStatusCB());

  // Queue enough frames to satisfy preroll.
  for (int i = 0; i < limits::kMaxVideoFrames; ++i)
    QueueNextFrame();

  SatisfyPendingRead();

  event.RunAndWaitForStatus(PIPELINE_OK);

  Play();

  Shutdown();
}

TEST_F(VideoRendererImplTest, Rebuffer_AlreadyHaveEnoughFrames) {
  Initialize();

  // Queue an extra frame so that we'll have enough frames to satisfy
  // preroll even after the first frame is painted.
  QueueNextFrame();
  Play();

  // Simulate a Pause/Preroll/Play rebuffer sequence.
  Pause();

  WaitableMessageLoopEvent event;
  renderer_->Preroll(kNoTimestamp(),
                     event.GetPipelineStatusCB());

  event.RunAndWaitForStatus(PIPELINE_OK);

  Play();

  Shutdown();
}

TEST_F(VideoRendererImplTest, GetCurrentFrame_Initialized) {
  Initialize();
  EXPECT_TRUE(GetCurrentFrame().get());  // Due to prerolling.
  Shutdown();
}

TEST_F(VideoRendererImplTest, GetCurrentFrame_Playing) {
  Initialize();
  Play();
  EXPECT_TRUE(GetCurrentFrame().get());
  Shutdown();
}

TEST_F(VideoRendererImplTest, GetCurrentFrame_Paused) {
  Initialize();
  Play();
  Pause();
  EXPECT_TRUE(GetCurrentFrame().get());
  Shutdown();
}

TEST_F(VideoRendererImplTest, GetCurrentFrame_Flushed) {
  Initialize();
  Play();
  Pause();

  // Frame shouldn't be updated.
  ResetCurrentFrame();
  Flush();
  EXPECT_FALSE(GetCurrentFrame().get());

  Shutdown();
}

TEST_F(VideoRendererImplTest, GetCurrentFrame_EndOfStream) {
  Initialize();
  Play();
  Pause();
  Flush();

  // Preroll only end of stream frames.
  QueueEndOfStream();

  // Frame shouldn't be updated.
  ResetCurrentFrame();
  Preroll(0, PIPELINE_OK);
  EXPECT_FALSE(GetCurrentFrame().get());

  // Start playing, we should immediately get notified of end of stream.
  Play();
  WaitForEnded();

  Shutdown();
}

TEST_F(VideoRendererImplTest, GetCurrentFrame_Shutdown) {
  Initialize();

  // Frame shouldn't be updated.
  ResetCurrentFrame();
  Shutdown();
  EXPECT_FALSE(GetCurrentFrame().get());
}

// Stop() is called immediately during an error.
TEST_F(VideoRendererImplTest, GetCurrentFrame_Error) {
  Initialize();

  // Frame shouldn't be updated.
  ResetCurrentFrame();
  Stop();
  EXPECT_FALSE(GetCurrentFrame().get());
}

// Verify that a late decoder response doesn't break invariants in the renderer.
TEST_F(VideoRendererImplTest, StopDuringOutstandingRead) {
  Initialize();
  Play();

  // Advance time a bit to trigger a Read().
  AdvanceTimeInMs(kFrameDurationInMs);
  WaitForPendingRead();

  WaitableMessageLoopEvent event;
  renderer_->Stop(event.GetClosure());

  event.RunAndWait();
}

TEST_F(VideoRendererImplTest, AbortPendingRead_Playing) {
  Initialize();
  Play();

  // Advance time a bit to trigger a Read().
  AdvanceTimeInMs(kFrameDurationInMs);
  WaitForPendingRead();

  QueueAbortedRead();
  SatisfyPendingRead();

  Pause();
  Flush();
  QueuePrerollFrames(kFrameDurationInMs * 6);
  Preroll(kFrameDurationInMs * 6, PIPELINE_OK);
  EXPECT_EQ(kFrameDurationInMs * 6, GetCurrentTimestampInMs());
  Shutdown();
}

TEST_F(VideoRendererImplTest, AbortPendingRead_Flush) {
  Initialize();
  Play();

  // Advance time a bit to trigger a Read().
  AdvanceTimeInMs(kFrameDurationInMs);
  WaitForPendingRead();

  Pause();
  Flush();
  Shutdown();
}

TEST_F(VideoRendererImplTest, AbortPendingRead_Preroll) {
  Initialize();
  Pause();
  Flush();

  QueueAbortedRead();
  Preroll(kFrameDurationInMs * 6, PIPELINE_OK);
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

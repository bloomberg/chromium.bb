// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_renderer_base.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "media/base/buffers.h"
#include "media/base/limits.h"
#include "media/base/pipeline.h"
#include "media/base/video_frame.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

base::TimeDelta VideoRendererBase::kMaxLastFrameDuration() {
  return base::TimeDelta::FromMilliseconds(250);
}

VideoRendererBase::VideoRendererBase(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const SetDecryptorReadyCB& set_decryptor_ready_cb,
    const PaintCB& paint_cb,
    const SetOpaqueCB& set_opaque_cb,
    bool drop_frames)
    : message_loop_(message_loop),
      weak_factory_(this),
      video_frame_stream_(message_loop, set_decryptor_ready_cb),
      received_end_of_stream_(false),
      frame_available_(&lock_),
      state_(kUninitialized),
      thread_(base::kNullThreadHandle),
      pending_read_(false),
      drop_frames_(drop_frames),
      playback_rate_(0),
      paint_cb_(paint_cb),
      set_opaque_cb_(set_opaque_cb),
      last_timestamp_(kNoTimestamp()) {
  DCHECK(!paint_cb_.is_null());
}

VideoRendererBase::~VideoRendererBase() {
  base::AutoLock auto_lock(lock_);
  CHECK(state_ == kStopped || state_ == kUninitialized) << state_;
  CHECK_EQ(thread_, base::kNullThreadHandle);
}

void VideoRendererBase::Play(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPrerolled, state_);
  state_ = kPlaying;
  callback.Run();
}

void VideoRendererBase::Pause(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ != kUninitialized || state_ == kError);
  state_ = kPaused;
  callback.Run();
}

void VideoRendererBase::Flush(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPaused);
  flush_cb_ = callback;
  state_ = kFlushingDecoder;

  video_frame_stream_.Reset(base::Bind(
      &VideoRendererBase::OnVideoFrameStreamResetDone, weak_this_));
}

void VideoRendererBase::Stop(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  if (state_ == kUninitialized || state_ == kStopped) {
    callback.Run();
    return;
  }

  // TODO(scherkus): Consider invalidating |weak_factory_| and replacing
  // task-running guards that check |state_| with DCHECK().

  state_ = kStopped;

  statistics_cb_.Reset();
  max_time_cb_.Reset();
  DoStopOrError_Locked();

  // Clean up our thread if present.
  base::PlatformThreadHandle thread_to_join = base::kNullThreadHandle;
  if (thread_ != base::kNullThreadHandle) {
    // Signal the thread since it's possible to get stopped with the video
    // thread waiting for a read to complete.
    frame_available_.Signal();
    std::swap(thread_, thread_to_join);
  }

  if (thread_to_join != base::kNullThreadHandle) {
    base::AutoUnlock auto_unlock(lock_);
    base::PlatformThread::Join(thread_to_join);
  }

  video_frame_stream_.Stop(callback);
}

void VideoRendererBase::SetPlaybackRate(float playback_rate) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
}

void VideoRendererBase::Preroll(base::TimeDelta time,
                                const PipelineStatusCB& cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kFlushed) << "Must flush prior to prerolling.";
  DCHECK(!cb.is_null());
  DCHECK(preroll_cb_.is_null());

  state_ = kPrerolling;
  preroll_cb_ = cb;
  preroll_timestamp_ = time;
  AttemptRead_Locked();
}

void VideoRendererBase::Initialize(const scoped_refptr<DemuxerStream>& stream,
                                   const VideoDecoderList& decoders,
                                   const PipelineStatusCB& init_cb,
                                   const StatisticsCB& statistics_cb,
                                   const TimeCB& max_time_cb,
                                   const NaturalSizeChangedCB& size_changed_cb,
                                   const base::Closure& ended_cb,
                                   const PipelineStatusCB& error_cb,
                                   const TimeDeltaCB& get_time_cb,
                                   const TimeDeltaCB& get_duration_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK(stream);
  DCHECK(!decoders.empty());
  DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
  DCHECK(!init_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!max_time_cb.is_null());
  DCHECK(!size_changed_cb.is_null());
  DCHECK(!ended_cb.is_null());
  DCHECK(!get_time_cb.is_null());
  DCHECK(!get_duration_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);

  weak_this_ = weak_factory_.GetWeakPtr();
  init_cb_ = init_cb;
  statistics_cb_ = statistics_cb;
  max_time_cb_ = max_time_cb;
  size_changed_cb_ = size_changed_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  get_time_cb_ = get_time_cb;
  get_duration_cb_ = get_duration_cb;
  state_ = kInitializing;

  video_frame_stream_.Initialize(
      stream,
      decoders,
      statistics_cb,
      base::Bind(&VideoRendererBase::OnVideoFrameStreamInitialized,
                 weak_this_));
}

void VideoRendererBase::OnVideoFrameStreamInitialized(bool success,
                                                      bool has_alpha) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  if (state_ == kStopped)
    return;

  DCHECK_EQ(state_, kInitializing);

  if (!success) {
    state_ = kUninitialized;
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // We're all good!  Consider ourselves flushed. (ThreadMain() should never
  // see us in the kUninitialized state).
  // Since we had an initial Preroll(), we consider ourself flushed, because we
  // have not populated any buffers yet.
  state_ = kFlushed;

  set_opaque_cb_.Run(!has_alpha);
  set_opaque_cb_.Reset();

  // Create our video thread.
  if (!base::PlatformThread::Create(0, this, &thread_)) {
    NOTREACHED() << "Video thread creation failed";
    state_ = kError;
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

#if defined(OS_WIN)
  // Bump up our priority so our sleeping is more accurate.
  // TODO(scherkus): find out if this is necessary, but it seems to help.
  ::SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
#endif  // defined(OS_WIN)
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

// PlatformThread::Delegate implementation.
void VideoRendererBase::ThreadMain() {
  base::PlatformThread::SetName("CrVideoRenderer");

  // The number of milliseconds to idle when we do not have anything to do.
  // Nothing special about the value, other than we're being more OS-friendly
  // than sleeping for 1 millisecond.
  //
  // TOOD(scherkus): switch to pure event-driven frame timing instead of this
  // kIdleTimeDelta business http://crbug.com/106874
  const base::TimeDelta kIdleTimeDelta =
      base::TimeDelta::FromMilliseconds(10);

  for (;;) {
    base::AutoLock auto_lock(lock_);

    // Thread exit condition.
    if (state_ == kStopped)
      return;

    // Remain idle as long as we're not playing.
    if (state_ != kPlaying || playback_rate_ == 0) {
      frame_available_.TimedWait(kIdleTimeDelta);
      continue;
    }

    // Remain idle until we have the next frame ready for rendering.
    if (ready_frames_.empty()) {
      if (received_end_of_stream_) {
        state_ = kEnded;
        ended_cb_.Run();

        // No need to sleep here as we idle when |state_ != kPlaying|.
        continue;
      }

      frame_available_.TimedWait(kIdleTimeDelta);
      continue;
    }

    base::TimeDelta remaining_time =
        CalculateSleepDuration(ready_frames_.front(), playback_rate_);

    // Sleep up to a maximum of our idle time until we're within the time to
    // render the next frame.
    if (remaining_time.InMicroseconds() > 0) {
      remaining_time = std::min(remaining_time, kIdleTimeDelta);
      frame_available_.TimedWait(remaining_time);
      continue;
    }

    // Deadline is defined as the midpoint between this frame and the next
    // frame, using the delta between this frame and the previous frame as the
    // assumption for frame duration.
    //
    // TODO(scherkus): An improvement over midpoint might be selecting the
    // minimum and/or maximum between the midpoint and some constants. As a
    // thought experiment, consider what would be better than the midpoint
    // for both the 1fps case and 120fps case.
    //
    // TODO(scherkus): This can be vastly improved. Use a histogram to measure
    // the accuracy of our frame timing code. http://crbug.com/149829
    if (drop_frames_ && last_timestamp_ != kNoTimestamp()) {
      base::TimeDelta now = get_time_cb_.Run();
      base::TimeDelta deadline = ready_frames_.front()->GetTimestamp() +
          (ready_frames_.front()->GetTimestamp() - last_timestamp_) / 2;

      if (now > deadline) {
        DropNextReadyFrame_Locked();
        continue;
      }
    }

    // Congratulations! You've made it past the video frame timing gauntlet.
    //
    // At this point enough time has passed that the next frame that ready for
    // rendering.
    PaintNextReadyFrame_Locked();
  }
}

void VideoRendererBase::PaintNextReadyFrame_Locked() {
  lock_.AssertAcquired();

  scoped_refptr<VideoFrame> next_frame = ready_frames_.front();
  ready_frames_.pop_front();

  last_timestamp_ = next_frame->GetTimestamp();

  const gfx::Size& natural_size = next_frame->natural_size();
  if (natural_size != last_natural_size_) {
    last_natural_size_ = natural_size;
    size_changed_cb_.Run(natural_size);
  }

  paint_cb_.Run(next_frame);

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoRendererBase::AttemptRead, weak_this_));
}

void VideoRendererBase::DropNextReadyFrame_Locked() {
  lock_.AssertAcquired();

  last_timestamp_ = ready_frames_.front()->GetTimestamp();
  ready_frames_.pop_front();

  PipelineStatistics statistics;
  statistics.video_frames_dropped = 1;
  statistics_cb_.Run(statistics);

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoRendererBase::AttemptRead, weak_this_));
}

void VideoRendererBase::FrameReady(VideoDecoder::Status status,
                                   const scoped_refptr<VideoFrame>& frame) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, kUninitialized);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status != VideoDecoder::kOk) {
    DCHECK(!frame);
    PipelineStatus error = PIPELINE_ERROR_DECODE;
    if (status == VideoDecoder::kDecryptError)
      error = PIPELINE_ERROR_DECRYPT;

    if (!preroll_cb_.is_null()) {
      base::ResetAndReturn(&preroll_cb_).Run(error);
      return;
    }

    error_cb_.Run(error);
    return;
  }

  // Already-queued Decoder ReadCB's can fire after various state transitions
  // have happened; in that case just drop those frames immediately.
  if (state_ == kStopped || state_ == kError || state_ == kFlushed ||
      state_ == kFlushingDecoder)
    return;

  if (state_ == kFlushing) {
    AttemptFlush_Locked();
    return;
  }

  if (!frame) {
    // Abort preroll early for a NULL frame because we won't get more frames.
    // A new preroll will be requested after this one completes so there is no
    // point trying to collect more frames.
    if (state_ == kPrerolling)
      TransitionToPrerolled_Locked();

    return;
  }

  if (frame->IsEndOfStream()) {
    DCHECK(!received_end_of_stream_);
    received_end_of_stream_ = true;
    max_time_cb_.Run(get_duration_cb_.Run());

    if (state_ == kPrerolling)
      TransitionToPrerolled_Locked();

    return;
  }

  // Maintain the latest frame decoded so the correct frame is displayed after
  // prerolling has completed.
  if (state_ == kPrerolling && frame->GetTimestamp() <= preroll_timestamp_) {
    ready_frames_.clear();
  }

  AddReadyFrame_Locked(frame);

  if (state_ == kPrerolling) {
    if (!video_frame_stream_.HasOutputFrameAvailable() ||
        ready_frames_.size() >= static_cast<size_t>(limits::kMaxVideoFrames)) {
      TransitionToPrerolled_Locked();
    }
  } else {
    // We only count frames decoded during normal playback.
    PipelineStatistics statistics;
    statistics.video_frames_decoded = 1;
    statistics_cb_.Run(statistics);
  }

  // Always request more decoded video if we have capacity. This serves two
  // purposes:
  //   1) Prerolling while paused
  //   2) Keeps decoding going if video rendering thread starts falling behind
  AttemptRead_Locked();
}

void VideoRendererBase::AddReadyFrame_Locked(
    const scoped_refptr<VideoFrame>& frame) {
  lock_.AssertAcquired();
  DCHECK(!frame->IsEndOfStream());

  // Adjust the incoming frame if its rendering stop time is past the duration
  // of the video itself. This is typically the last frame of the video and
  // occurs if the container specifies a duration that isn't a multiple of the
  // frame rate.  Another way for this to happen is for the container to state
  // a smaller duration than the largest packet timestamp.
  base::TimeDelta duration = get_duration_cb_.Run();
  if (frame->GetTimestamp() > duration) {
    frame->SetTimestamp(duration);
  }

  ready_frames_.push_back(frame);
  DCHECK_LE(ready_frames_.size(),
            static_cast<size_t>(limits::kMaxVideoFrames));

  max_time_cb_.Run(frame->GetTimestamp());

  // Avoid needlessly waking up |thread_| unless playing.
  if (state_ == kPlaying)
    frame_available_.Signal();
}

void VideoRendererBase::AttemptRead() {
  base::AutoLock auto_lock(lock_);
  AttemptRead_Locked();
}

void VideoRendererBase::AttemptRead_Locked() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (pending_read_ || received_end_of_stream_ ||
      ready_frames_.size() == static_cast<size_t>(limits::kMaxVideoFrames)) {
    return;
  }

  switch (state_) {
    case kPaused:
    case kFlushing:
    case kPrerolling:
    case kPlaying:
      pending_read_ = true;
      video_frame_stream_.ReadFrame(base::Bind(&VideoRendererBase::FrameReady,
                                               weak_this_));
      return;

    case kUninitialized:
    case kInitializing:
    case kPrerolled:
    case kFlushingDecoder:
    case kFlushed:
    case kEnded:
    case kStopped:
    case kError:
      return;
  }
}

void VideoRendererBase::OnVideoFrameStreamResetDone() {
  base::AutoLock auto_lock(lock_);
  if (state_ == kStopped)
    return;

  DCHECK_EQ(kFlushingDecoder, state_);
  DCHECK(!pending_read_);

  state_ = kFlushing;
  AttemptFlush_Locked();
}

void VideoRendererBase::AttemptFlush_Locked() {
  lock_.AssertAcquired();
  DCHECK_EQ(kFlushing, state_);

  ready_frames_.clear();
  received_end_of_stream_ = false;

  if (pending_read_)
    return;

  state_ = kFlushed;
  last_timestamp_ = kNoTimestamp();
  base::ResetAndReturn(&flush_cb_).Run();
}

base::TimeDelta VideoRendererBase::CalculateSleepDuration(
    const scoped_refptr<VideoFrame>& next_frame,
    float playback_rate) {
  // Determine the current and next presentation timestamps.
  base::TimeDelta now = get_time_cb_.Run();
  base::TimeDelta next_pts = next_frame->GetTimestamp();

  // Scale our sleep based on the playback rate.
  base::TimeDelta sleep = next_pts - now;
  return base::TimeDelta::FromMicroseconds(
      static_cast<int64>(sleep.InMicroseconds() / playback_rate));
}

void VideoRendererBase::DoStopOrError_Locked() {
  lock_.AssertAcquired();
  last_timestamp_ = kNoTimestamp();
  ready_frames_.clear();
}

void VideoRendererBase::TransitionToPrerolled_Locked() {
  lock_.AssertAcquired();
  DCHECK_EQ(state_, kPrerolling);

  state_ = kPrerolled;

  // Because we might remain in the prerolled state for an undetermined amount
  // of time (e.g., we seeked while paused), we'll paint the first prerolled
  // frame.
  if (!ready_frames_.empty())
    PaintNextReadyFrame_Locked();

  base::ResetAndReturn(&preroll_cb_).Run(PIPELINE_OK);
}

}  // namespace media

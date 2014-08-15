// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_renderer_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "media/base/buffers.h"
#include "media/base/limits.h"
#include "media/base/pipeline.h"
#include "media/base/video_frame.h"

namespace media {

VideoRendererImpl::VideoRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    ScopedVector<VideoDecoder> decoders,
    const SetDecryptorReadyCB& set_decryptor_ready_cb,
    const PaintCB& paint_cb,
    bool drop_frames)
    : task_runner_(task_runner),
      video_frame_stream_(new VideoFrameStream(task_runner,
                                               decoders.Pass(),
                                               set_decryptor_ready_cb)),
      low_delay_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      frame_available_(&lock_),
      state_(kUninitialized),
      thread_(),
      pending_read_(false),
      drop_frames_(drop_frames),
      buffering_state_(BUFFERING_HAVE_NOTHING),
      paint_cb_(paint_cb),
      last_timestamp_(kNoTimestamp()),
      last_painted_timestamp_(kNoTimestamp()),
      frames_decoded_(0),
      frames_dropped_(0),
      is_shutting_down_(false),
      weak_factory_(this) {
  DCHECK(!paint_cb_.is_null());
}

VideoRendererImpl::~VideoRendererImpl() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(lock_);
    is_shutting_down_ = true;
    frame_available_.Signal();
  }

  if (!thread_.is_null())
    base::PlatformThread::Join(thread_);

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_ABORT);

  if (!flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run();
}

void VideoRendererImpl::Flush(const base::Closure& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);
  flush_cb_ = callback;
  state_ = kFlushing;

  // This is necessary if the |video_frame_stream_| has already seen an end of
  // stream and needs to drain it before flushing it.
  ready_frames_.clear();
  if (buffering_state_ != BUFFERING_HAVE_NOTHING) {
    buffering_state_ = BUFFERING_HAVE_NOTHING;
    buffering_state_cb_.Run(BUFFERING_HAVE_NOTHING);
  }
  received_end_of_stream_ = false;
  rendered_end_of_stream_ = false;

  video_frame_stream_->Reset(
      base::Bind(&VideoRendererImpl::OnVideoFrameStreamResetDone,
                 weak_factory_.GetWeakPtr()));
}

void VideoRendererImpl::StartPlaying() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kFlushed);
  DCHECK(!pending_read_);
  DCHECK(ready_frames_.empty());
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);

  state_ = kPlaying;
  start_timestamp_ = get_time_cb_.Run();
  AttemptRead_Locked();
}

void VideoRendererImpl::Initialize(DemuxerStream* stream,
                                   bool low_delay,
                                   const PipelineStatusCB& init_cb,
                                   const StatisticsCB& statistics_cb,
                                   const TimeCB& max_time_cb,
                                   const BufferingStateCB& buffering_state_cb,
                                   const base::Closure& ended_cb,
                                   const PipelineStatusCB& error_cb,
                                   const TimeDeltaCB& get_time_cb,
                                   const TimeDeltaCB& get_duration_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
  DCHECK(!init_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!max_time_cb.is_null());
  DCHECK(!buffering_state_cb.is_null());
  DCHECK(!ended_cb.is_null());
  DCHECK(!get_time_cb.is_null());
  DCHECK(!get_duration_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);

  low_delay_ = low_delay;

  init_cb_ = init_cb;
  statistics_cb_ = statistics_cb;
  max_time_cb_ = max_time_cb;
  buffering_state_cb_ = buffering_state_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  get_time_cb_ = get_time_cb;
  get_duration_cb_ = get_duration_cb;
  state_ = kInitializing;

  video_frame_stream_->Initialize(
      stream,
      low_delay,
      statistics_cb,
      base::Bind(&VideoRendererImpl::OnVideoFrameStreamInitialized,
                 weak_factory_.GetWeakPtr()));
}

void VideoRendererImpl::OnVideoFrameStreamInitialized(bool success) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
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

  // Create our video thread.
  CHECK(base::PlatformThread::Create(0, this, &thread_));

#if defined(OS_WIN)
  // Bump up our priority so our sleeping is more accurate.
  // TODO(scherkus): find out if this is necessary, but it seems to help.
  ::SetThreadPriority(thread_.platform_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif  // defined(OS_WIN)
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

// PlatformThread::Delegate implementation.
void VideoRendererImpl::ThreadMain() {
  base::PlatformThread::SetName("CrVideoRenderer");

  // The number of milliseconds to idle when we do not have anything to do.
  // Nothing special about the value, other than we're being more OS-friendly
  // than sleeping for 1 millisecond.
  //
  // TODO(scherkus): switch to pure event-driven frame timing instead of this
  // kIdleTimeDelta business http://crbug.com/106874
  const base::TimeDelta kIdleTimeDelta =
      base::TimeDelta::FromMilliseconds(10);

  // If we have no frames and haven't painted any frame for certain amount of
  // time, declare BUFFERING_HAVE_NOTHING.
  const base::TimeDelta kTimeToDeclareHaveNothing =
      base::TimeDelta::FromSeconds(3);

  for (;;) {
    base::AutoLock auto_lock(lock_);

    // Thread exit condition.
    if (is_shutting_down_)
      return;

    // Remain idle as long as we're not playing.
    if (state_ != kPlaying || buffering_state_ != BUFFERING_HAVE_ENOUGH) {
      UpdateStatsAndWait_Locked(kIdleTimeDelta);
      continue;
    }

    base::TimeDelta now = get_time_cb_.Run();

    // Remain idle until we have the next frame ready for rendering.
    if (ready_frames_.empty()) {
      if (received_end_of_stream_) {
        if (!rendered_end_of_stream_) {
          rendered_end_of_stream_ = true;
          task_runner_->PostTask(FROM_HERE, ended_cb_);
        }
      } else if (last_painted_timestamp_ != kNoTimestamp() &&
                 now - last_painted_timestamp_ >= kTimeToDeclareHaveNothing) {
        buffering_state_ = BUFFERING_HAVE_NOTHING;
        task_runner_->PostTask(
            FROM_HERE, base::Bind(buffering_state_cb_, BUFFERING_HAVE_NOTHING));
      }

      UpdateStatsAndWait_Locked(kIdleTimeDelta);
      continue;
    }

    base::TimeDelta target_paint_timestamp = ready_frames_.front()->timestamp();
    base::TimeDelta latest_paint_timestamp;

    // Deadline is defined as the duration between this frame and the next
    // frame, using the delta between this frame and the previous frame as the
    // assumption for frame duration.
    //
    // TODO(scherkus): This can be vastly improved. Use a histogram to measure
    // the accuracy of our frame timing code. http://crbug.com/149829
    if (last_timestamp_ == kNoTimestamp()) {
      latest_paint_timestamp = base::TimeDelta::Max();
    } else {
      base::TimeDelta duration = target_paint_timestamp - last_timestamp_;
      latest_paint_timestamp = target_paint_timestamp + duration;
    }

    // Remain idle until we've reached our target paint window.
    if (now < target_paint_timestamp) {
      UpdateStatsAndWait_Locked(kIdleTimeDelta);
      continue;
    }

    if (now > latest_paint_timestamp && drop_frames_) {
      DropNextReadyFrame_Locked();
      continue;
    }

    // Congratulations! You've made it past the video frame timing gauntlet.
    //
    // At this point enough time has passed that the next frame that ready for
    // rendering.
    PaintNextReadyFrame_Locked();
  }
}

void VideoRendererImpl::PaintNextReadyFrame_Locked() {
  lock_.AssertAcquired();

  scoped_refptr<VideoFrame> next_frame = ready_frames_.front();
  ready_frames_.pop_front();
  frames_decoded_++;

  last_timestamp_ = next_frame->timestamp();
  last_painted_timestamp_ = next_frame->timestamp();

  paint_cb_.Run(next_frame);

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoRendererImpl::AttemptRead, weak_factory_.GetWeakPtr()));
}

void VideoRendererImpl::DropNextReadyFrame_Locked() {
  TRACE_EVENT0("media", "VideoRendererImpl:frameDropped");

  lock_.AssertAcquired();

  last_timestamp_ = ready_frames_.front()->timestamp();
  ready_frames_.pop_front();
  frames_decoded_++;
  frames_dropped_++;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoRendererImpl::AttemptRead, weak_factory_.GetWeakPtr()));
}

void VideoRendererImpl::FrameReady(VideoFrameStream::Status status,
                                   const scoped_refptr<VideoFrame>& frame) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kFlushed);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status == VideoFrameStream::DECODE_ERROR ||
      status == VideoFrameStream::DECRYPT_ERROR) {
    DCHECK(!frame.get());
    PipelineStatus error = PIPELINE_ERROR_DECODE;
    if (status == VideoFrameStream::DECRYPT_ERROR)
      error = PIPELINE_ERROR_DECRYPT;
    error_cb_.Run(error);
    return;
  }

  // Already-queued VideoFrameStream ReadCB's can fire after various state
  // transitions have happened; in that case just drop those frames immediately.
  if (state_ == kFlushing)
    return;

  DCHECK_EQ(state_, kPlaying);

  // Can happen when demuxers are preparing for a new Seek().
  if (!frame) {
    DCHECK_EQ(status, VideoFrameStream::DEMUXER_READ_ABORTED);
    return;
  }

  if (frame->end_of_stream()) {
    DCHECK(!received_end_of_stream_);
    received_end_of_stream_ = true;
  } else {
    // Maintain the latest frame decoded so the correct frame is displayed after
    // prerolling has completed.
    if (frame->timestamp() <= start_timestamp_)
      ready_frames_.clear();
    AddReadyFrame_Locked(frame);
  }

  // Signal buffering state if we've met our conditions for having enough data.
  if (buffering_state_ != BUFFERING_HAVE_ENOUGH && HaveEnoughData_Locked())
    TransitionToHaveEnough_Locked();

  // Always request more decoded video if we have capacity. This serves two
  // purposes:
  //   1) Prerolling while paused
  //   2) Keeps decoding going if video rendering thread starts falling behind
  AttemptRead_Locked();
}

bool VideoRendererImpl::HaveEnoughData_Locked() {
  DCHECK_EQ(state_, kPlaying);
  return received_end_of_stream_ ||
      !video_frame_stream_->CanReadWithoutStalling() ||
      ready_frames_.size() >= static_cast<size_t>(limits::kMaxVideoFrames) ||
      (low_delay_ && ready_frames_.size() > 0);
}

void VideoRendererImpl::TransitionToHaveEnough_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);

  if (received_end_of_stream_)
    max_time_cb_.Run(get_duration_cb_.Run());

  if (!ready_frames_.empty()) {
    // Max time isn't reported while we're in a have nothing state as we could
    // be discarding frames to find |start_timestamp_|.
    if (!received_end_of_stream_) {
      base::TimeDelta max_timestamp = ready_frames_[0]->timestamp();
      for (size_t i = 1; i < ready_frames_.size(); ++i) {
        if (ready_frames_[i]->timestamp() > max_timestamp)
          max_timestamp = ready_frames_[i]->timestamp();
      }
      max_time_cb_.Run(max_timestamp);
    }

    // Because the clock might remain paused in for an undetermined amount
    // of time (e.g., seeking while paused), paint the first frame.
    PaintNextReadyFrame_Locked();
  }

  buffering_state_ = BUFFERING_HAVE_ENOUGH;
  buffering_state_cb_.Run(BUFFERING_HAVE_ENOUGH);
}

void VideoRendererImpl::AddReadyFrame_Locked(
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();
  DCHECK(!frame->end_of_stream());

  // Adjust the incoming frame if its rendering stop time is past the duration
  // of the video itself. This is typically the last frame of the video and
  // occurs if the container specifies a duration that isn't a multiple of the
  // frame rate.  Another way for this to happen is for the container to state
  // a smaller duration than the largest packet timestamp.
  base::TimeDelta duration = get_duration_cb_.Run();
  if (frame->timestamp() > duration) {
    frame->set_timestamp(duration);
  }

  ready_frames_.push_back(frame);
  DCHECK_LE(ready_frames_.size(),
            static_cast<size_t>(limits::kMaxVideoFrames));

  // FrameReady() may add frames but discard them when we're decoding frames to
  // reach |start_timestamp_|. In this case we'll only want to update the max
  // time when we know we've reached |start_timestamp_| and have buffered enough
  // frames to being playback.
  if (buffering_state_ == BUFFERING_HAVE_ENOUGH)
    max_time_cb_.Run(frame->timestamp());

  // Avoid needlessly waking up |thread_| unless playing.
  if (state_ == kPlaying)
    frame_available_.Signal();
}

void VideoRendererImpl::AttemptRead() {
  base::AutoLock auto_lock(lock_);
  AttemptRead_Locked();
}

void VideoRendererImpl::AttemptRead_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (pending_read_ || received_end_of_stream_ ||
      ready_frames_.size() == static_cast<size_t>(limits::kMaxVideoFrames)) {
    return;
  }

  switch (state_) {
    case kPlaying:
      pending_read_ = true;
      video_frame_stream_->Read(base::Bind(&VideoRendererImpl::FrameReady,
                                           weak_factory_.GetWeakPtr()));
      return;

    case kUninitialized:
    case kInitializing:
    case kFlushing:
    case kFlushed:
      return;
  }
}

void VideoRendererImpl::OnVideoFrameStreamResetDone() {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kFlushing, state_);
  DCHECK(!pending_read_);
  DCHECK(ready_frames_.empty());
  DCHECK(!received_end_of_stream_);
  DCHECK(!rendered_end_of_stream_);
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);

  state_ = kFlushed;
  last_timestamp_ = kNoTimestamp();
  last_painted_timestamp_ = kNoTimestamp();
  base::ResetAndReturn(&flush_cb_).Run();
}

void VideoRendererImpl::UpdateStatsAndWait_Locked(
    base::TimeDelta wait_duration) {
  lock_.AssertAcquired();
  DCHECK_GE(frames_decoded_, 0);
  DCHECK_LE(frames_dropped_, frames_decoded_);

  if (frames_decoded_) {
    PipelineStatistics statistics;
    statistics.video_frames_decoded = frames_decoded_;
    statistics.video_frames_dropped = frames_dropped_;
    task_runner_->PostTask(FROM_HERE, base::Bind(statistics_cb_, statistics));

    frames_decoded_ = 0;
    frames_dropped_ = 0;
  }

  frame_available_.TimedWait(wait_duration);
}

}  // namespace media

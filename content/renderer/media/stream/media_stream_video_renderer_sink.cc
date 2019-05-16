// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_video_renderer_sink.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/child/child_process.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_util.h"

const int kMinFrameSize = 2;

namespace content {

// FrameDeliverer is responsible for delivering frames received on
// OnVideoFrame() to |repaint_cb_| on the IO thread.
//
// It is created on the main thread, but methods should be called and class
// should be destructed on the IO thread.
class MediaStreamVideoRendererSink::FrameDeliverer {
 public:
  FrameDeliverer(
      const RepaintCB& repaint_cb,
      base::RepeatingCallback<void(media::VideoCaptureFrameDropReason)>
          frame_dropped_cb,
      scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner)
      : main_render_task_runner_(std::move(main_render_task_runner)),
        repaint_cb_(repaint_cb),
        frame_dropped_cb_(std::move(frame_dropped_cb)),
        state_(STOPPED),
        frame_size_(kMinFrameSize, kMinFrameSize),
        emit_frame_drop_events_(true) {
    DETACH_FROM_THREAD(io_thread_checker_);
  }

  ~FrameDeliverer() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    DCHECK(state_ == STARTED || state_ == PAUSED) << state_;
  }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame,
                    base::TimeTicks /*current_time*/) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    DCHECK(frame);
    TRACE_EVENT_INSTANT1("webrtc",
                         "MediaStreamVideoRendererSink::"
                         "FrameDeliverer::OnVideoFrame",
                         TRACE_EVENT_SCOPE_THREAD, "timestamp",
                         frame->timestamp().InMilliseconds());

    if (state_ != STARTED) {
      if (emit_frame_drop_events_) {
        emit_frame_drop_events_ = false;
        main_render_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(frame_dropped_cb_,
                           media::VideoCaptureFrameDropReason::
                               kRendererSinkFrameDelivererIsNotStarted));
      }
      return;
    }

    frame_size_ = frame->natural_size();
    repaint_cb_.Run(std::move(frame));
  }

  void RenderEndOfStream() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    // This is necessary to make sure audio can play if the video tag src is a
    // MediaStream video track that has been rejected or ended. It also ensure
    // that the renderer doesn't hold a reference to a real video frame if no
    // more frames are provided. This is since there might be a finite number
    // of available buffers. E.g, video that originates from a video camera.
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateBlackFrame(
            state_ == STOPPED ? gfx::Size(kMinFrameSize, kMinFrameSize)
                              : frame_size_);
    video_frame->metadata()->SetBoolean(
        media::VideoFrameMetadata::END_OF_STREAM, true);
    video_frame->metadata()->SetTimeTicks(
        media::VideoFrameMetadata::REFERENCE_TIME, base::TimeTicks::Now());
    OnVideoFrame(video_frame, base::TimeTicks());
  }

  void Start() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    DCHECK_EQ(state_, STOPPED);
    SetState(STARTED);
  }

  void Resume() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    if (state_ == PAUSED)
      SetState(STARTED);
  }

  void Pause() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    if (state_ == STARTED)
      SetState(PAUSED);
  }

 private:
  void SetState(State target_state) {
    state_ = target_state;
    emit_frame_drop_events_ = true;
  }

  friend class MediaStreamVideoRendererSink;

  const scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner_;
  const RepaintCB repaint_cb_;
  const base::RepeatingCallback<void(media::VideoCaptureFrameDropReason)>
      frame_dropped_cb_;
  State state_;
  gfx::Size frame_size_;
  bool emit_frame_drop_events_;

  // Used for DCHECKs to ensure method calls are executed on the correct thread.
  THREAD_CHECKER(io_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(FrameDeliverer);
};

MediaStreamVideoRendererSink::MediaStreamVideoRendererSink(
    const blink::WebMediaStreamTrack& video_track,
    const RepaintCB& repaint_cb,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner)
    : repaint_cb_(repaint_cb),
      video_track_(video_track),
      io_task_runner_(std::move(io_task_runner)),
      main_render_task_runner_(std::move(main_render_task_runner)),
      weak_factory_(this) {}

MediaStreamVideoRendererSink::~MediaStreamVideoRendererSink() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
}

void MediaStreamVideoRendererSink::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  frame_deliverer_.reset(new MediaStreamVideoRendererSink::FrameDeliverer(
      repaint_cb_,
      base::BindRepeating(&blink::MediaStreamVideoSink::OnFrameDropped,
                          weak_factory_.GetWeakPtr()),
      main_render_task_runner_));
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FrameDeliverer::Start,
                                base::Unretained(frame_deliverer_.get())));

  blink::MediaStreamVideoSink::ConnectToTrack(
      video_track_,
      // This callback is run on IO thread. It is safe to use base::Unretained
      // here because |frame_receiver_| will be destroyed on IO thread after
      // sink is disconnected from track.
      base::Bind(&FrameDeliverer::OnVideoFrame,
                 base::Unretained(frame_deliverer_.get())),
      // Local display video rendering is considered a secure link.
      true);

  if (video_track_.Source().GetReadyState() ==
          blink::WebMediaStreamSource::kReadyStateEnded ||
      !video_track_.IsEnabled()) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FrameDeliverer::RenderEndOfStream,
                                  base::Unretained(frame_deliverer_.get())));
  }
}

void MediaStreamVideoRendererSink::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  blink::MediaStreamVideoSink::DisconnectFromTrack();
  if (frame_deliverer_)
    io_task_runner_->DeleteSoon(FROM_HERE, frame_deliverer_.release());
}

void MediaStreamVideoRendererSink::Resume() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!frame_deliverer_)
    return;

  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FrameDeliverer::Resume,
                                base::Unretained(frame_deliverer_.get())));
}

void MediaStreamVideoRendererSink::Pause() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!frame_deliverer_)
    return;

  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FrameDeliverer::Pause,
                                base::Unretained(frame_deliverer_.get())));
}

void MediaStreamVideoRendererSink::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (state == blink::WebMediaStreamSource::kReadyStateEnded &&
      frame_deliverer_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FrameDeliverer::RenderEndOfStream,
                                  base::Unretained(frame_deliverer_.get())));
  }
}

MediaStreamVideoRendererSink::State
MediaStreamVideoRendererSink::GetStateForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!frame_deliverer_)
    return STOPPED;
  return frame_deliverer_->state_;
}

}  // namespace content

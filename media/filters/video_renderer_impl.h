// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_RENDERER_IMPL_H_
#define MEDIA_FILTERS_VIDEO_RENDERER_IMPL_H_

#include <deque>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/base/video_renderer.h"
#include "media/filters/decoder_stream.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// VideoRendererImpl creates its own thread for the sole purpose of timing frame
// presentation.  It handles reading from the VideoFrameStream and stores the
// results in a queue of decoded frames and executing a callback when a frame is
// ready for rendering.
class MEDIA_EXPORT VideoRendererImpl
    : public VideoRenderer,
      public base::PlatformThread::Delegate {
 public:
  typedef base::Callback<void(const scoped_refptr<VideoFrame>&)> PaintCB;

  // |decoders| contains the VideoDecoders to use when initializing.
  //
  // |paint_cb| is executed on the video frame timing thread whenever a new
  // frame is available for painting.
  //
  // Implementors should avoid doing any sort of heavy work in this method and
  // instead post a task to a common/worker thread to handle rendering.  Slowing
  // down the video thread may result in losing synchronization with audio.
  //
  // Setting |drop_frames_| to true causes the renderer to drop expired frames.
  VideoRendererImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      ScopedVector<VideoDecoder> decoders,
      const SetDecryptorReadyCB& set_decryptor_ready_cb,
      const PaintCB& paint_cb,
      bool drop_frames);
  virtual ~VideoRendererImpl();

  // VideoRenderer implementation.
  virtual void Initialize(DemuxerStream* stream,
                          bool low_delay,
                          const PipelineStatusCB& init_cb,
                          const StatisticsCB& statistics_cb,
                          const TimeCB& max_time_cb,
                          const BufferingStateCB& buffering_state_cb,
                          const base::Closure& ended_cb,
                          const PipelineStatusCB& error_cb,
                          const TimeDeltaCB& get_time_cb,
                          const TimeDeltaCB& get_duration_cb) OVERRIDE;
  virtual void Flush(const base::Closure& callback) OVERRIDE;
  virtual void StartPlaying() OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;

  // PlatformThread::Delegate implementation.
  virtual void ThreadMain() OVERRIDE;

 private:
  // Callback for |video_frame_stream_| initialization.
  void OnVideoFrameStreamInitialized(bool success);

  // Callback for |video_frame_stream_| to deliver decoded video frames and
  // report video decoding status.
  void FrameReady(VideoFrameStream::Status status,
                  const scoped_refptr<VideoFrame>& frame);

  // Helper method for adding a frame to |ready_frames_|.
  void AddReadyFrame_Locked(const scoped_refptr<VideoFrame>& frame);

  // Helper method that schedules an asynchronous read from the
  // |video_frame_stream_| as long as there isn't a pending read and we have
  // capacity.
  void AttemptRead();
  void AttemptRead_Locked();

  // Called when VideoFrameStream::Reset() completes.
  void OnVideoFrameStreamResetDone();

  // Helper function that flushes the buffers when a Stop() or error occurs.
  void DoStopOrError_Locked();

  // Runs |paint_cb_| with the next frame from |ready_frames_|.
  //
  // A read is scheduled to replace the frame.
  void PaintNextReadyFrame_Locked();

  // Drops the next frame from |ready_frames_| and runs |statistics_cb_|.
  //
  // A read is scheduled to replace the frame.
  void DropNextReadyFrame_Locked();

  // Returns true if the renderer has enough data for playback purposes.
  // Note that having enough data may be due to reaching end of stream.
  bool HaveEnoughData_Locked();
  void TransitionToHaveEnough_Locked();

  // Runs |statistics_cb_| with |frames_decoded_| and |frames_dropped_|, resets
  // them to 0, and then waits on |frame_available_| for up to the
  // |wait_duration|.
  void UpdateStatsAndWait_Locked(base::TimeDelta wait_duration);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Used for accessing data members.
  base::Lock lock_;

  // Provides video frames to VideoRendererImpl.
  scoped_ptr<VideoFrameStream> video_frame_stream_;

  // Flag indicating low-delay mode.
  bool low_delay_;

  // Queue of incoming frames yet to be painted.
  typedef std::deque<scoped_refptr<VideoFrame> > VideoFrameQueue;
  VideoFrameQueue ready_frames_;

  // Keeps track of whether we received the end of stream buffer and finished
  // rendering.
  bool received_end_of_stream_;
  bool rendered_end_of_stream_;

  // Used to signal |thread_| as frames are added to |frames_|.  Rule of thumb:
  // always check |state_| to see if it was set to STOPPED after waking up!
  base::ConditionVariable frame_available_;

  // Important detail: being in kPlaying doesn't imply that video is being
  // rendered. Rather, it means that the renderer is ready to go. The actual
  // rendering of video is controlled by time advancing via |time_cb_|.
  //
  //   kUninitialized
  //         | Initialize()
  //         |
  //         V
  //    kInitializing
  //         | Decoders initialized
  //         |
  //         V            Decoders reset
  //      kFlushed <------------------ kFlushing
  //         | StartPlaying()             ^
  //         |                            |
  //         |                            | Flush()
  //         `---------> kPlaying --------'
  enum State {
    kUninitialized,
    kInitializing,
    kFlushing,
    kFlushed,
    kPlaying,
    kStopped,
  };
  State state_;

  // Video thread handle.
  base::PlatformThreadHandle thread_;

  // Keep track of the outstanding read on the VideoFrameStream. Flushing can
  // only complete once the read has completed.
  bool pending_read_;

  bool drop_frames_;

  BufferingState buffering_state_;

  // Playback operation callbacks.
  base::Closure flush_cb_;

  // Event callbacks.
  PipelineStatusCB init_cb_;
  StatisticsCB statistics_cb_;
  TimeCB max_time_cb_;
  BufferingStateCB buffering_state_cb_;
  base::Closure ended_cb_;
  PipelineStatusCB error_cb_;
  TimeDeltaCB get_time_cb_;
  TimeDeltaCB get_duration_cb_;

  base::TimeDelta start_timestamp_;

  // Embedder callback for notifying a new frame is available for painting.
  PaintCB paint_cb_;

  // The timestamp of the last frame removed from the |ready_frames_| queue,
  // either for calling |paint_cb_| or for dropping. Set to kNoTimestamp()
  // during flushing.
  base::TimeDelta last_timestamp_;

  // Keeps track of the number of frames decoded and dropped since the
  // last call to |statistics_cb_|. These must be accessed under lock.
  int frames_decoded_;
  int frames_dropped_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoRendererImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_RENDERER_IMPL_H_

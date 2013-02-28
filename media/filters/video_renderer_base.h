// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
#define MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_

#include <deque>

#include "base/memory/ref_counted.h"
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

namespace base {
class MessageLoopProxy;
}

namespace media {

class DecryptingDemuxerStream;
class VideoDecoderSelector;

// VideoRendererBase creates its own thread for the sole purpose of timing frame
// presentation.  It handles reading from the decoder and stores the results in
// a queue of decoded frames and executing a callback when a frame is ready for
// rendering.
class MEDIA_EXPORT VideoRendererBase
    : public VideoRenderer,
      public base::PlatformThread::Delegate {
 public:
  typedef base::Callback<void(const scoped_refptr<VideoFrame>&)> PaintCB;
  typedef base::Callback<void(bool)> SetOpaqueCB;

  // Maximum duration of the last frame.
  static base::TimeDelta kMaxLastFrameDuration();

  // |paint_cb| is executed on the video frame timing thread whenever a new
  // frame is available for painting.
  //
  // |set_opaque_cb| is executed when the renderer is initialized to inform
  // the player whether the decoder's output will be opaque or not.
  //
  // Implementors should avoid doing any sort of heavy work in this method and
  // instead post a task to a common/worker thread to handle rendering.  Slowing
  // down the video thread may result in losing synchronization with audio.
  //
  // Setting |drop_frames_| to true causes the renderer to drop expired frames.
  //
  // TODO(scherkus): pass the VideoFrame* to this callback and remove
  // Get/PutCurrentFrame() http://crbug.com/108435
  VideoRendererBase(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                    const SetDecryptorReadyCB& set_decryptor_ready_cb,
                    const PaintCB& paint_cb,
                    const SetOpaqueCB& set_opaque_cb,
                    bool drop_frames);
  virtual ~VideoRendererBase();

  // VideoRenderer implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const VideoDecoderList& decoders,
                          const PipelineStatusCB& init_cb,
                          const StatisticsCB& statistics_cb,
                          const TimeCB& max_time_cb,
                          const NaturalSizeChangedCB& size_changed_cb,
                          const base::Closure& ended_cb,
                          const PipelineStatusCB& error_cb,
                          const TimeDeltaCB& get_time_cb,
                          const TimeDeltaCB& get_duration_cb) OVERRIDE;
  virtual void Play(const base::Closure& callback) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Flush(const base::Closure& callback) OVERRIDE;
  virtual void Preroll(base::TimeDelta time,
                       const PipelineStatusCB& cb) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;

  // PlatformThread::Delegate implementation.
  virtual void ThreadMain() OVERRIDE;

 private:
  // Called when |decoder_selector_| selected the |selected_decoder|.
  // |decrypting_demuxer_stream| was also populated if a DecryptingDemuxerStream
  // created to help decrypt the encrypted stream.
  // Note: |decoder_selector| is passed here to keep the VideoDecoderSelector
  // alive until OnDecoderSelected() finishes.
  void OnDecoderSelected(
      scoped_ptr<VideoDecoderSelector> decoder_selector,
      const scoped_refptr<VideoDecoder>& selected_decoder,
      const scoped_refptr<DecryptingDemuxerStream>& decrypting_demuxer_stream);

  // Callback from the video decoder delivering decoded video frames and
  // reporting video decoder status.
  void FrameReady(VideoDecoder::Status status,
                  const scoped_refptr<VideoFrame>& frame);

  // Helper method for adding a frame to |ready_frames_|.
  void AddReadyFrame_Locked(const scoped_refptr<VideoFrame>& frame);

  // Helper method that schedules an asynchronous read from the decoder as long
  // as there isn't a pending read and we have capacity.
  void AttemptRead();
  void AttemptRead_Locked();

  // Called when VideoDecoder::Reset() completes.
  void OnDecoderResetDone();

  // Attempts to complete flushing and transition into the flushed state.
  void AttemptFlush_Locked();

  // Calculates the duration to sleep for based on |last_timestamp_|,
  // the next frame timestamp (may be NULL), and the provided playback rate.
  //
  // We don't use |playback_rate_| to avoid locking.
  base::TimeDelta CalculateSleepDuration(
      const scoped_refptr<VideoFrame>& next_frame,
      float playback_rate);

  // Helper function that flushes the buffers when a Stop() or error occurs.
  void DoStopOrError_Locked();

  // Runs |paint_cb_| with the next frame from |ready_frames_|, updating
  // |last_natural_size_| and running |size_changed_cb_| if the natural size
  // changes.
  //
  // A read is scheduled to replace the frame.
  void PaintNextReadyFrame_Locked();

  // Drops the next frame from |ready_frames_| and runs |statistics_cb_|.
  //
  // A read is scheduled to replace the frame.
  void DropNextReadyFrame_Locked();

  void ResetDecoder();
  void StopDecoder(const base::Closure& callback);

  // Pops the front of |decoders|, assigns it to |decoder_| and then
  // calls initialize on the new decoder.
  void InitializeNextDecoder(const scoped_refptr<DemuxerStream>& demuxer_stream,
                             scoped_ptr<VideoDecoderList> decoders);

  // Called when |decoder_| initialization completes.
  // |demuxer_stream| & |decoders| are used if initialization failed and
  // InitializeNextDecoder() needs to be called again.
  void OnDecoderInitDone(const scoped_refptr<DemuxerStream>& demuxer_stream,
                         scoped_ptr<VideoDecoderList> decoders,
                         PipelineStatus status);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WeakPtrFactory<VideoRendererBase> weak_factory_;
  base::WeakPtr<VideoRendererBase> weak_this_;

  // Used for accessing data members.
  base::Lock lock_;

  SetDecryptorReadyCB set_decryptor_ready_cb_;

  // These two will be set by VideoDecoderSelector::SelectVideoDecoder().
  scoped_refptr<VideoDecoder> decoder_;
  scoped_refptr<DecryptingDemuxerStream> decrypting_demuxer_stream_;

  // Queue of incoming frames yet to be painted.
  typedef std::deque<scoped_refptr<VideoFrame> > VideoFrameQueue;
  VideoFrameQueue ready_frames_;

  // Keeps track of whether we received the end of stream buffer.
  bool received_end_of_stream_;

  // Used to signal |thread_| as frames are added to |frames_|.  Rule of thumb:
  // always check |state_| to see if it was set to STOPPED after waking up!
  base::ConditionVariable frame_available_;

  // State transition Diagram of this class:
  //       [kUninitialized] -------> [kError]
  //              |
  //              | Initialize()
  //        [kInitializing]
  //              |
  //              V        All frames returned
  //   +------[kFlushed]<-----[kFlushing]<--- OnDecoderResetDone()
  //   |          | Preroll() or upon                  ^
  //   |          V got first frame           [kFlushingDecoder]
  //   |      [kPrerolling]                            ^
  //   |          |                                 | Flush()
  //   |          V Got enough frames               |
  //   |      [kPrerolled]---------------------->[kPaused]
  //   |          |                Pause()          ^
  //   |          V Play()                          |
  //   |       [kPlaying]---------------------------|
  //   |          |                Pause()          ^
  //   |          V Receive EOF frame.              | Pause()
  //   |       [kEnded]-----------------------------+
  //   |                                            ^
  //   |                                            |
  //   +-----> [kStopped]                   [Any state other than]
  //                                        [kUninitialized/kError]

  // Simple state tracking variable.
  enum State {
    kUninitialized,
    kInitializing,
    kPrerolled,
    kPaused,
    kFlushingDecoder,
    kFlushing,
    kFlushed,
    kPrerolling,
    kPlaying,
    kEnded,
    kStopped,
    kError,
  };
  State state_;

  // Video thread handle.
  base::PlatformThreadHandle thread_;

  // Keep track of outstanding reads on the video decoder. Flushing can only
  // complete once reads have completed.
  bool pending_read_;

  bool drop_frames_;

  float playback_rate_;

  // Playback operation callbacks.
  base::Closure flush_cb_;
  PipelineStatusCB preroll_cb_;

  // Event callbacks.
  PipelineStatusCB init_cb_;
  StatisticsCB statistics_cb_;
  TimeCB max_time_cb_;
  NaturalSizeChangedCB size_changed_cb_;
  base::Closure ended_cb_;
  PipelineStatusCB error_cb_;
  TimeDeltaCB get_time_cb_;
  TimeDeltaCB get_duration_cb_;

  base::TimeDelta preroll_timestamp_;

  // Embedder callback for notifying a new frame is available for painting.
  PaintCB paint_cb_;

  // Callback to execute to inform the player if the video decoder's output is
  // opaque.
  SetOpaqueCB set_opaque_cb_;

  // The last natural size |size_changed_cb_| was called with.
  //
  // TODO(scherkus): WebMediaPlayerImpl should track this instead of plumbing
  // this through Pipeline. The one tricky bit might be guaranteeing we deliver
  // the size information before we reach HAVE_METADATA.
  gfx::Size last_natural_size_;

  // The timestamp of the last frame removed from the |ready_frames_| queue,
  // either for calling |paint_cb_| or for dropping. Set to kNoTimestamp()
  // during flushing.
  base::TimeDelta last_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_

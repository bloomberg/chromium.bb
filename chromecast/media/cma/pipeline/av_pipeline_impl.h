// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_AV_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BASE_AV_PIPELINE_IMPL_H_

#include <list>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/base/cast_decoder_buffer_impl.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/stream_id.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {
class BrowserCdmCast;
class BufferingFrameProvider;
class BufferingState;
class CodedFrameProvider;
class DecoderBufferBase;

class AvPipelineImpl {
 public:
  // Pipeline states.
  enum State {
    kUninitialized,
    kPlaying,
    kFlushing,
    kFlushed,
    kStopped,
    kError,
  };

  typedef base::Callback<
      void(StreamId id,
           const ::media::AudioDecoderConfig&,
           const ::media::VideoDecoderConfig&)> UpdateConfigCB;

  AvPipelineImpl(MediaPipelineBackend::Decoder* decoder,
                 const UpdateConfigCB& update_config_cb);
  ~AvPipelineImpl();

  // Setting the frame provider or the client must be done in the
  // |kUninitialized| state.
  void SetCodedFrameProvider(scoped_ptr<CodedFrameProvider> frame_provider,
                             size_t max_buffer_size,
                             size_t max_frame_size);

  // Setup the pipeline and ensure samples are available for the given media
  // time, then start rendering samples.
  bool StartPlayingFrom(base::TimeDelta time,
                        const scoped_refptr<BufferingState>& buffering_state);

  // Flush any remaining samples in the pipeline.
  // Invoke |done_cb| when flush is completed.
  void Flush(const base::Closure& done_cb);

  // Tear down the pipeline and release the hardware resources.
  void Stop();

  State GetState() const { return state_; }
  void TransitionToState(State state);

  void SetCdm(BrowserCdmCast* media_keys);

  void OnBufferPushed(MediaPipelineBackend::BufferStatus status);

 private:
  // Callback invoked when the CDM state has changed in a way that might
  // impact media playback.
  void OnCdmStateChange();

  // Feed the pipeline, getting the frames from |frame_provider_|.
  void FetchBufferIfNeeded();

  // Callback invoked when receiving a new frame from |frame_provider_|.
  void OnNewFrame(const scoped_refptr<DecoderBufferBase>& buffer,
                  const ::media::AudioDecoderConfig& audio_config,
                  const ::media::VideoDecoderConfig& video_config);

  // Process a pending buffer.
  void ProcessPendingBuffer();

  // Callbacks:
  // - when BrowserCdm updated its state.
  // - when BrowserCdm has been destroyed.
  void OnCdmStateChanged();
  void OnCdmDestroyed();

  // Callback invoked when a media buffer has been buffered by |frame_provider_|
  // which is a BufferingFrameProvider.
  void OnDataBuffered(const scoped_refptr<DecoderBufferBase>& buffer,
                      bool is_at_max_capacity);
  void UpdatePlayableFrames();

  base::ThreadChecker thread_checker_;

  UpdateConfigCB update_config_cb_;

  // Backends.
  MediaPipelineBackend::Decoder* decoder_;

  // AV pipeline state.
  State state_;

  // Buffering state.
  // Can be NULL if there is no buffering strategy.
  scoped_refptr<BufferingState> buffering_state_;

  // |buffered_time_| is the maximum timestamp of buffered frames.
  // |playable_buffered_time_| is the maximum timestamp of buffered and
  // playable frames (i.e. the key id is available for those frames).
  base::TimeDelta buffered_time_;
  base::TimeDelta playable_buffered_time_;

  // List of frames buffered but not playable right away due to a missing
  // key id.
  std::list<scoped_refptr<DecoderBufferBase> > non_playable_frames_;

  // Buffer provider.
  scoped_ptr<BufferingFrameProvider> frame_provider_;

  // Indicate whether the frame fetching process is active.
  bool enable_feeding_;

  // Indicate whether there is a pending buffer read.
  bool pending_read_;

  // Pending buffer (not pushed to device yet)
  scoped_refptr<DecoderBufferBase> pending_buffer_;

  // Buffer that has been pushed to the device but not processed yet.
  CastDecoderBufferImpl pushed_buffer_;

  // The media time is retrieved at regular intervals.
  // Indicate whether time update is enabled.
  bool enable_time_update_;
  bool pending_time_update_task_;

  // Decryption keys, if available.
  BrowserCdmCast* media_keys_;
  int media_keys_callback_id_;

  base::WeakPtr<AvPipelineImpl> weak_this_;
  base::WeakPtrFactory<AvPipelineImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AvPipelineImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_AV_PIPELINE_IMPL_H_

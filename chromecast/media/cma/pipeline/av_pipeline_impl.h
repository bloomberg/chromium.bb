// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_AV_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BASE_AV_PIPELINE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/stream_id.h"
#include "media/base/pipeline_status.h"

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

class AvPipelineImpl : MediaPipelineBackend::Decoder::Delegate {
 public:
  AvPipelineImpl(MediaPipelineBackend::Decoder* decoder,
                 const AvPipelineClient& client);
  ~AvPipelineImpl() override;

  void SetCdm(BrowserCdmCast* media_keys);

  // Setup the pipeline and ensure samples are available for the given media
  // time, then start rendering samples.
  bool StartPlayingFrom(base::TimeDelta time,
                        const scoped_refptr<BufferingState>& buffering_state);

  bool StartFlush();
  void Flush(const ::media::PipelineStatusCB& status_cb);
  // Resets any pending buffers after the backend has been stopped.
  void BackendStopped();

  // Tear down the pipeline and release the hardware resources.
  void Stop();

  virtual void UpdateStatistics() = 0;

 protected:
  // Pipeline states.
  enum State {
    kUninitialized,
    kPlaying,
    kFlushing,
    kFlushed,
    kStopped,
    kError,
  };

  State state() const { return state_; }
  const AvPipelineClient& client() const { return client_; }

  virtual void OnUpdateConfig(
      StreamId id,
      const ::media::AudioDecoderConfig& audio_config,
      const ::media::VideoDecoderConfig& video_config) = 0;

  void TransitionToState(State state);
  // Setting the frame provider must be done in the |kUninitialized| state.
  void SetCodedFrameProvider(scoped_ptr<CodedFrameProvider> frame_provider,
                             size_t max_buffer_size,
                             size_t max_frame_size);

  ::media::PipelineStatistics previous_stats_;

 private:
  void OnFlushDone(const ::media::PipelineStatusCB& status_cb);

  // MediaPipelineBackend::Decoder::Delegate implementation:
  void OnPushBufferComplete(BufferStatus status) override;
  void OnEndOfStream() override;
  void OnDecoderError() override;
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override;
  void OnVideoResolutionChanged(const Size& size) override;

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

  MediaPipelineBackend::Decoder* const decoder_;
  const AvPipelineClient client_;

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
  scoped_refptr<DecoderBufferBase> pushed_buffer_;

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

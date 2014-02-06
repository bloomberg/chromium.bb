// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_AUDIO_TRACK_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_AUDIO_TRACK_HOST_H_

#include <deque>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/renderer/pepper/pepper_media_stream_track_host_base.h"
#include "media/audio/audio_parameters.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace content {

class PepperMediaStreamAudioTrackHost : public PepperMediaStreamTrackHostBase {
 public:
  PepperMediaStreamAudioTrackHost(RendererPpapiHost* host,
                                  PP_Instance instance,
                                  PP_Resource resource,
                                  const blink::WebMediaStreamTrack& track);

 private:
  // A helper class for receiving audio samples in the audio thread.
  // This class is created and destroyed on the renderer main thread.
  class AudioSink : public MediaStreamAudioSink {
   public:
    explicit AudioSink(PepperMediaStreamAudioTrackHost* host);
    virtual ~AudioSink();

    // Enqueues a free frame index into |frames_| which will be used for
    // sending audio samples to plugin.
    // This function is called on the renderer main thread.
    void EnqueueFrame(int32_t index);

   private:
    void InitFramesOnMainThread(int32_t number_of_frames, int32_t frame_size);
    void SendEnqueueFrameMessageOnMainThread(int32_t index);

    // MediaStreamAudioSink overrides:
    // These two functions should be called on the audio thread.
    virtual void OnData(const int16* audio_data,
                        int sample_rate,
                        int number_of_channels,
                        int number_of_frames) OVERRIDE;
    virtual void OnSetFormat(const media::AudioParameters& params) OVERRIDE;

    // Unowned host which is available during the AudioSink's lifespan.
    // It is mainly used in the main thread. But the audio thread will use
    // host_->frame_buffer() to read some buffer properties. It is safe
    // because the frame_buffer()'s properties will not be changed after
    // initialization.
    PepperMediaStreamAudioTrackHost* host_;

    // Timestamp of the next received audio frame.
    // Access only on the audio thread.
    base::TimeDelta timestamp_;

    // Duration of one audio frame.
    // Access only on the audio thread.
    base::TimeDelta frame_duration_;

    // The current audio parameters.
    // Access only on the audio thread.
    media::AudioParameters audio_params_;

    // The original audio parameters which is set in the first time of
    // OnSetFormat being called.
    // Access only on the audio thread.
    media::AudioParameters original_audio_params_;

    // The size of a frame in bytes.
    // Access only on the audio thread.
    uint32_t frame_data_size_;

    // A lock to protect the index queue |frames_|.
    base::Lock lock_;

    // A queue for free frame indices.
    std::deque<int32_t> frames_;

    scoped_refptr<base::MessageLoopProxy> main_message_loop_proxy_;

    base::ThreadChecker audio_thread_checker_;

    base::WeakPtrFactory<AudioSink> weak_factory_;

    DISALLOW_COPY_AND_ASSIGN(AudioSink);
  };

  virtual ~PepperMediaStreamAudioTrackHost();

  // PepperMediaStreamTrackHostBase overrides:
  virtual void OnClose() OVERRIDE;

  // MediaStreamFrameBuffer::Delegate overrides:
  virtual void OnNewFrameEnqueued() OVERRIDE;

  // ResourceHost overrides:
  virtual void DidConnectPendingHostToResource() OVERRIDE;

  blink::WebMediaStreamTrack track_;

  // True if |audio_sink_| has been added to |blink::WebMediaStreamTrack|
  // as a sink.
  bool connected_;

  AudioSink audio_sink_;

  DISALLOW_COPY_AND_ASSIGN(PepperMediaStreamAudioTrackHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_AUDIO_TRACK_HOST_H_

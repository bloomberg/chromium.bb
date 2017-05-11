// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/backend/android/media_pipeline_backend_android.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

class DecoderBufferBase;
class AudioSinkAndroidAudioTrackImpl;

// Input handle to the sink. All methods (including constructor and destructor)
// must be called on the same thread.
class AudioSinkAndroid {
 public:
  enum class SinkError {
    // This input is being ignored due to a sample rate changed.
    kInputIgnored,
    // An internal sink error occurred. The input is no longer usable.
    kInternalError,
  };

  class Delegate {
   public:
    using SinkError = AudioSinkAndroid::SinkError;

    // Called when the last data passed to WritePcm() has been successfully
    // added to the queue.
    virtual void OnWritePcmCompletion(
        MediaPipelineBackendAndroid::BufferStatus status,
        const MediaPipelineBackendAndroid::RenderingDelay& delay) = 0;

    // Called when a sink error occurs. No further data should be written.
    virtual void OnSinkError(SinkError error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  AudioSinkAndroid(Delegate* delegate,
                   int samples_per_second,
                   bool primary,
                   const std::string& device_id,
                   AudioContentType content_type);
  ~AudioSinkAndroid();

  // Writes some PCM data to the sink. |data| must be in planar float format.
  // Once the data has been written, the delegate's OnWritePcmCompletion()
  // method will be called on the same thread that the AudioSinkAndroid was
  // created on. Note that no further calls to WritePcm() should be made until
  // OnWritePcmCompletion() has been called.
  void WritePcm(const scoped_refptr<DecoderBufferBase>& data);

  // Pauses/unpauses this input.
  void SetPaused(bool paused);

  // Sets the volume multiplier for this input. If |multiplier| is outside the
  // range [0.0, 1.0], it is clamped to that range.
  void SetVolumeMultiplier(float multiplier);

 private:
  std::unique_ptr<AudioSinkAndroidAudioTrackImpl> impl_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioSinkAndroid);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_H_

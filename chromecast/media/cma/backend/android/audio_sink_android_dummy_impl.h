// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_DUMMY_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_DUMMY_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/media/cma/backend/android/audio_sink_android.h"
#include "chromecast/media/cma/backend/android/media_pipeline_backend_android.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {

class AudioSinkAndroidDummyImpl {
 public:
  enum State {
    kStateUninitialized,   // No data has been queued yet.
    kStateNormalPlayback,  // Normal playback.
    kStatePaused,          // Currently paused.
    kStateGotEos,          // Got the end-of-stream buffer (normal playback).
    kStateDeleted,         // Told the mixer to delete this.
    kStateError,           // A mixer error occurred, this is unusable now.
  };

  AudioSinkAndroidDummyImpl(AudioSinkAndroid::Delegate* delegate,
                            int input_samples_per_second,
                            bool primary,
                            const std::string& device_id,
                            AudioContentType content_type);

  ~AudioSinkAndroidDummyImpl();

  // Queues some PCM data to be mixed. |data| must be in planar float format.
  void WritePcm(const scoped_refptr<DecoderBufferBase>& data);

  // Sets the pause state of this stream.
  void SetPaused(bool paused);

  // Sets the volume multiplier for this stream. If |multiplier| < 0, sets the
  // volume multiplier to 0. If |multiplier| > 1, sets the volume multiplier
  // to 1.
  void SetVolumeMultiplier(float multiplier);

  // Prevents any further calls to the delegate (ie, called when the delegate
  // is being destroyed).
  void PreventDelegateCalls();

  State state() const { return state_; }

 private:
  bool IsDeleting() const;

  void PostPcmCallback(
      const MediaPipelineBackendAndroid::RenderingDelay& delay);

  AudioSinkAndroid::Delegate* const delegate_;
  const int input_samples_per_second_;
  const bool primary_;
  const std::string device_id_;
  const AudioContentType content_type_;
  const scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  State state_;

  float stream_volume_multiplier_;

  base::WeakPtr<AudioSinkAndroidDummyImpl> weak_this_;
  base::WeakPtrFactory<AudioSinkAndroidDummyImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioSinkAndroidDummyImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_DUMMY_IMPL_H_

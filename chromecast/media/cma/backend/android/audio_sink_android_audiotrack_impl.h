// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_AUDIOTRACK_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_AUDIOTRACK_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "chromecast/media/cma/backend/android/audio_sink_android.h"
#include "chromecast/media/cma/backend/android/media_pipeline_backend_android.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {

class AudioSinkAndroidAudioTrackImpl {
 public:
  enum State {
    kStateUninitialized,   // No data has been queued yet.
    kStateNormalPlayback,  // Normal playback.
    kStatePaused,          // Currently paused.
    kStateGotEos,          // Got the end-of-stream buffer (normal playback).
    kStateError,           // A sink error occurred, this is unusable now.
  };

  // TODO(ckuiper): There doesn't seem to be a maximum size for the buffers
  // sent through the media pipeline, so we need to add code to break up a
  // buffer larger than this size and feed it in in smaller chunks.
  static const int kDirectBufferSize = 512 * 1024;

  AudioSinkAndroidAudioTrackImpl(AudioSinkAndroid::Delegate* delegate,
                                 int input_samples_per_second,
                                 bool primary,
                                 const std::string& device_id,
                                 AudioContentType content_type);

  ~AudioSinkAndroidAudioTrackImpl();

  static bool RegisterJni(JNIEnv* env);

  // Called from Java so that we can cache the addresses of the Java-managed
  // byte_buffers.
  void CacheDirectBufferAddress(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& pcm_byte_buffer,
      const base::android::JavaParamRef<jobject>& timestamp_byte_buffer);

  // Feeds data through JNI into the AudioTrack. The data must be in planar
  // float format.
  void WritePcm(scoped_refptr<DecoderBufferBase> data);

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
  void FinalizeOnFeederThread();

  void FeedData();
  void FeedDataContinue();

  // Reformats audio data from planar float into interleaved float for
  // AudioTrack. I.e.:
  // "LLLLLLLLLLLLLLLLRRRRRRRRRRRRRRRR" -> "LRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLR".
  void ReformatData();

  void TrackRawMonotonicClockDeviation();

  void PostPcmCallback(
      const MediaPipelineBackendAndroid::RenderingDelay& delay);

  void SignalError(AudioSinkAndroid::SinkError error);
  void PostError(AudioSinkAndroid::SinkError error);

  AudioSinkAndroid::Delegate* const delegate_;
  const int input_samples_per_second_;
  const bool primary_;
  const std::string device_id_;
  const AudioContentType content_type_;

  // Java AudioSinkAudioTrackImpl instance.
  base::android::ScopedJavaGlobalRef<jobject> j_audio_sink_audiotrack_impl_;

  // Thread that feeds audio data into the Java instance though JNI,
  // potentially blocking. When in Play mode the Java AudioTrack blocks as it
  // waits for queue space to become available for the new data. In Pause mode
  // it returns immediately once all queue space has been filled up. This case
  // is handled separately via FeedDataContinue().
  base::Thread feeder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> feeder_task_runner_;

  const scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Buffers shared between native and Java space to move data across the JNI.
  // We use direct buffers so that the native class can have access to the
  // underlying memory address. This avoids the need to copy from a jbyteArray
  // to native memory. More discussion of this here:
  // http://developer.android.com/training/articles/perf-jni.html Owned by
  // j_audio_sink_audiotrack_impl_.
  uint8_t* direct_pcm_buffer_address_;  // PCM audio data native->java
  // rendering delay+timestamp return value, java->native
  uint64_t* direct_rendering_delay_address_;

  State state_;

  float stream_volume_multiplier_;

  scoped_refptr<DecoderBufferBase> pending_data_;
  int pending_data_bytes_already_fed_;

  MediaPipelineBackendAndroid::RenderingDelay sink_rendering_delay_;

  base::WeakPtr<AudioSinkAndroidAudioTrackImpl> weak_this_;
  base::WeakPtrFactory<AudioSinkAndroidAudioTrackImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioSinkAndroidAudioTrackImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_AUDIOTRACK_IMPL_H_

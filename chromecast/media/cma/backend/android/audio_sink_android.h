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

const int kDefaultSlewTimeMs = 15;

const char* GetAudioContentTypeName(const AudioContentType type);

class DecoderBufferBase;

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

  enum SinkType {
    kSinkTypeJavaBased,   // Java-based (using AudioTrack)
    kSinkTypeNativeBased  // Native-based (not implemented yet)
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

  AudioSinkAndroid() {}
  virtual ~AudioSinkAndroid() {}

  // Writes some PCM data to the sink. |data| must be in planar float format.
  // Once the data has been written, the delegate's OnWritePcmCompletion()
  // method will be called on the same thread that the AudioSinkAndroid was
  // created on. Note that no further calls to WritePcm() should be made until
  // OnWritePcmCompletion() has been called.
  virtual void WritePcm(scoped_refptr<DecoderBufferBase> data) = 0;

  // Pauses/unpauses this input.
  virtual void SetPaused(bool paused) = 0;

  // Sets the volume multiplier for this input. If |multiplier| is outside the
  // range [0.0, 1.0], it is clamped to that range.
  virtual void SetVolumeMultiplier(float multiplier) = 0;

  // Sets the multiplier based on this stream's content type. The resulting
  // output volume should be the content type volume * the per-stream volume
  // multiplier. If |fade_ms| is >= 0, the volume change should be faded over
  // that many milliseconds; otherwise, the default fade time should be used.
  virtual void SetContentTypeVolume(float multiplier, int fade_ms) = 0;

  // Sets whether or not this stream should be muted.
  virtual void SetMuted(bool muted) = 0;

  // Returns the volume multiplier of the stream.
  virtual float EffectiveVolume() const = 0;

  // Getters
  virtual int input_samples_per_second() const = 0;
  virtual bool primary() const = 0;
  virtual std::string device_id() const = 0;
  virtual AudioContentType content_type() const = 0;
  virtual const char* GetContentTypeName() const = 0;
};

// Implementation of "managed" AudioSinkAndroid* object that is
// automatically added to the Sink Manager when created and removed when
// destroyed. Inspired by std::unique_ptr<>.
class ManagedAudioSink {
 public:
  using SinkType = AudioSinkAndroid::SinkType;
  using Delegate = AudioSinkAndroid::Delegate;

  explicit ManagedAudioSink(SinkType sink_type);
  ~ManagedAudioSink();

  // Resets the sink_ object by removing it from the manager and deleting it.
  void Reset();

  // Resets the sink_ object to a new AudioSinkAndroid* instance and adds it to
  // the manager. If a valid instance existed on entry it is removed from the
  // manager and deleted before creating the new one.
  void Reset(Delegate* delegate,
             int samples_per_second,
             bool primary,
             const std::string& device_id,
             AudioContentType content_type);

  AudioSinkAndroid* operator->() const { return sink_; }
  operator AudioSinkAndroid*() const { return sink_; }

 private:
  void Remove();

  SinkType sink_type_;
  AudioSinkAndroid* sink_;

  DISALLOW_COPY_AND_ASSIGN(ManagedAudioSink);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_H_

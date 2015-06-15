// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_RENDERER_SINK_H_
#define MEDIA_BASE_AUDIO_RENDERER_SINK_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_output_ipc.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/media_export.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

typedef base::Callback<void(SwitchOutputDeviceResult)> SwitchOutputDeviceCB;

// AudioRendererSink is an interface representing the end-point for
// rendered audio.  An implementation is expected to
// periodically call Render() on a callback object.

class AudioRendererSink
    : public base::RefCountedThreadSafe<media::AudioRendererSink> {
 public:
  class RenderCallback {
   public:
    // Attempts to completely fill all channels of |dest|, returns actual
    // number of frames filled.
    virtual int Render(AudioBus* dest, int audio_delay_milliseconds) = 0;

    // Signals an error has occurred.
    virtual void OnRenderError() = 0;

   protected:
    virtual ~RenderCallback() {}
  };

  // Sets important information about the audio stream format.
  // It must be called before any of the other methods.
  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* callback) = 0;

  // Starts audio playback.
  virtual void Start() = 0;

  // Stops audio playback.
  virtual void Stop() = 0;

  // Pauses playback.
  virtual void Pause() = 0;

  // Resumes playback after calling Pause().
  virtual void Play() = 0;

  // Sets the playback volume, with range [0.0, 1.0] inclusive.
  // Returns |true| on success.
  virtual bool SetVolume(double volume) = 0;

  // Attempts to switch the audio output device.
  // Once the attempt is finished, |callback| is invoked with the
  // result of the operation passed as a parameter. The result is a value from
  // the  media::SwitchOutputDeviceResult enum.
  // There is no guarantee about the thread where |callback| will
  // be invoked, so users are advised to use media::BindToCurrentLoop() to
  // ensure that |callback| runs on the correct thread.
  // Note also that copy constructors and destructors for arguments bound to
  // |callback| may run on arbitrary threads as |callback| is moved across
  // threads. It is advisable to bind arguments such that they are released by
  // |callback| when it runs in order to avoid surprises.
  virtual void SwitchOutputDevice(const std::string& device_id,
                                  const GURL& security_origin,
                                  const SwitchOutputDeviceCB& callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<AudioRendererSink>;
  virtual ~AudioRendererSink() {}
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_SINK_H_

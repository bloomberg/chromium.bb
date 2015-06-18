// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_AUDIO_RENDERER_H_
#define CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_AUDIO_RENDERER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/audio_renderer_sink.h"
#include "url/gurl.h"

namespace content {

class MediaStreamAudioRenderer
    : public base::RefCountedThreadSafe<MediaStreamAudioRenderer> {
 public:
  // Starts rendering audio.
  virtual void Start() = 0;

  // Stops rendering audio.
  virtual void Stop() = 0;

  // Resumes rendering audio after being paused.
  virtual void Play() = 0;

  // Temporarily suspends rendering audio. The audio stream might still be
  // active but new audio data is not provided to the consumer.
  virtual void Pause() = 0;

  // Sets the output volume.
  virtual void SetVolume(float volume) = 0;

  // Attempts to switch the audio output device.
  // Once the attempt is finished, |callback| is invoked with the
  // result of the operation passed as a parameter. The result is a value from
  // the media::SwitchOutputDeviceResult enum.
  // There is no guarantee about the thread where |callback| will
  // be invoked, so users are advised to use media::BindToCurrentLoop() to
  // ensure that |callback| runs on the correct thread.
  // Note also that copy constructors and destructors for arguments bound to
  // |callback| may run on arbitrary threads as |callback| is moved across
  // threads. It is advisable to bind arguments such that they are released by
  // |callback| when it runs in order to avoid surprises.
  virtual void SwitchOutputDevice(
      const std::string& device_id,
      const GURL& security_origin,
      const media::SwitchOutputDeviceCB& callback) = 0;

  // Time stamp that reflects the current render time. Should not be updated
  // when paused.
  virtual base::TimeDelta GetCurrentRenderTime() const = 0;

  // Returns true if the implementation is a local renderer and false
  // otherwise.
  virtual bool IsLocalRenderer() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<MediaStreamAudioRenderer>;

  virtual ~MediaStreamAudioRenderer() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_AUDIO_RENDERER_H_

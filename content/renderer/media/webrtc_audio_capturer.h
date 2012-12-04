// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_

#include <list>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/audio/audio_input_device.h"
#include "media/base/audio_capturer_source.h"

namespace content {

class WebRtcAudioCapturerSink;

// This class manages the capture data flow by getting data from its
// |source_|, and passing it to its |sink_|.
// It allows clients to inject their own capture data source by calling
// SetCapturerSource().
class WebRtcAudioCapturer
    : public base::RefCountedThreadSafe<WebRtcAudioCapturer>,
      public media::AudioCapturerSource::CaptureCallback,
      public media::AudioCapturerSource::CaptureEventHandler {
 public:
  // Use to construct the audio capturer.
  static scoped_refptr<WebRtcAudioCapturer> CreateCapturer();

  // Called by the client on the sink side. Return false if the capturer has
  // not been initialized successfully.
  void AddCapturerSink(WebRtcAudioCapturerSink* sink);

  // Called by the client on the sink side to remove
  void RemoveCapturerSink(WebRtcAudioCapturerSink* sink);

  // SetCapturerSource() is called if client on the source side desires to
  // provide their own captured audio data. Client is responsible for calling
  // Start() on its own source to have the ball rolling.
  void SetCapturerSource(
      const scoped_refptr<media::AudioCapturerSource>& source);

  // Starts recording audio.
  void Start();

  // Stops recording audio.
  void Stop();

  // Sets the microphone volume.
  void SetVolume(double volume);

  // Specifies the |session_id| to query which device to use.
  void SetDevice(int session_id);

  // Enables or disables the WebRtc AGC control.
  void SetAutomaticGainControl(bool enable);

  bool is_recording() const { return running_; }

  // AudioCapturerSource::CaptureCallback implementation.
  virtual void Capture(media::AudioBus* audio_source,
                       int audio_delay_milliseconds,
                       double volume) OVERRIDE;
  virtual void OnCaptureError() OVERRIDE;

  // AudioCapturerSource::CaptureEventHandler implementation.
  virtual void OnDeviceStarted(const std::string& device_id) OVERRIDE;
  virtual void OnDeviceStopped() OVERRIDE;

 protected:
  friend class base::RefCountedThreadSafe<WebRtcAudioCapturer>;
  virtual ~WebRtcAudioCapturer();

 private:
  typedef std::list<WebRtcAudioCapturerSink*> SinkList;

  WebRtcAudioCapturer();

  // Initializes the capturer, called right after the object is created.
  // Returns false if the initializetion fails.
  bool Initialize();

  // A list of sinks that the audio data is fed to.
  SinkList sinks_;

  // The audio data source from the browser process.
  scoped_refptr<media::AudioCapturerSource> source_;

  // Cached values of utilized audio parameters. Platform dependent.
  media::AudioParameters params_;

  // Buffers used for temporary storage during capture callbacks.
  // Allocated during initialization.
  scoped_array<int16> buffer_;
  std::string device_id_;
  bool running_;

  // Protect access to |source_|, |sinks_|, |running_|.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioCapturer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_

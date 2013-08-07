// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_

#include <list>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/audio/audio_input_device.h"
#include "media/base/audio_capturer_source.h"

namespace media {
class AudioBus;
}

namespace content {

class WebRtcLocalAudioRenderer;
class WebRtcLocalAudioTrack;

// This class manages the capture data flow by getting data from its
// |source_|, and passing it to its |tracks_|.
// It allows clients to inject their own capture data source by calling
// SetCapturerSource().
// The threading model for this class is rather complex since it will be
// created on the main render thread, captured data is provided on a dedicated
// AudioInputDevice thread, and methods can be called either on the Libjingle
// thread or on the main render thread but also other client threads
// if an alternative AudioCapturerSource has been set.
class CONTENT_EXPORT WebRtcAudioCapturer
    : public base::RefCountedThreadSafe<WebRtcAudioCapturer>,
      NON_EXPORTED_BASE(public media::AudioCapturerSource::CaptureCallback) {
 public:
  // Use to construct the audio capturer.
  // Called on the main render thread.
  static scoped_refptr<WebRtcAudioCapturer> CreateCapturer();

  // Creates and configures the default audio capturing source using the
  // provided audio parameters.  |render_view_id| specifies the render view
  // consuming audio for capture.  |session_id| is passed to the browser to
  // decide which device to use.  |device_id| is used to identify which device
  // the capturer is created for.  Called on the main render thread.
  bool Initialize(int render_view_id,
                  media::ChannelLayout channel_layout,
                  int sample_rate,
                  int session_id,
                  const std::string& device_id);

  // Add a audio track to the sinks of the capturer.
  // WebRtcAudioDeviceImpl calls this method on the main render thread but
  // other clients may call it from other threads. The current implementation
  // does not support multi-thread calling.
  // Called on the main render thread or libjingle working thread.
  void AddTrack(WebRtcLocalAudioTrack* track);

  // Remove a audio track from the sinks of the capturer.
  // Called on the main render thread or libjingle working thread.
  void RemoveTrack(WebRtcLocalAudioTrack* track);

  // SetCapturerSource() is called if the client on the source side desires to
  // provide their own captured audio data. Client is responsible for calling
  // Start() on its own source to have the ball rolling.
  // Called on the main render thread.
  void SetCapturerSource(
      const scoped_refptr<media::AudioCapturerSource>& source,
      media::ChannelLayout channel_layout,
      float sample_rate);

  // Volume APIs used by WebRtcAudioDeviceImpl.
  // Called on the AudioInputDevice audio thread.
  void SetVolume(int volume);
  int Volume() const;
  int MaxVolume() const;

  // Enables or disables the WebRtc AGC control.
  // Called from a Libjingle working thread.
  void SetAutomaticGainControl(bool enable);

  bool is_recording() const { return running_; }

  // Audio parameters utilized by the audio capturer. Can be utilized by
  // a local renderer to set up a renderer using identical parameters as the
  // capturer.
  // TODO(phoglund): This accessor is inherently unsafe since the returned
  // parameters can become outdated at any time. Think over the implications
  // of this accessor and if we can remove it.
  media::AudioParameters audio_parameters() const;

  const std::string& device_id() const { return device_id_; }

 protected:
  friend class base::RefCountedThreadSafe<WebRtcAudioCapturer>;
  WebRtcAudioCapturer();
  virtual ~WebRtcAudioCapturer();

 private:
  class TrackOwner;
  typedef std::list<scoped_refptr<TrackOwner> > TrackList;

  // AudioCapturerSource::CaptureCallback implementation.
  // Called on the AudioInputDevice audio thread.
  virtual void Capture(media::AudioBus* audio_source,
                       int audio_delay_milliseconds,
                       double volume) OVERRIDE;
  virtual void OnCaptureError() OVERRIDE;

  // Reconfigures the capturer with a new buffer size and capture parameters.
  // Must be called without holding the lock. Returns true on success.
  bool Reconfigure(int sample_rate, media::ChannelLayout channel_layout);

  // Starts recording audio.
  // Triggered by AddSink() on the main render thread or a Libjingle working
  // thread. It should NOT be called under |lock_|.
  void Start();

  // Stops recording audio.
  // Triggered by RemoveSink() on the main render thread or a Libjingle working
  // thread. It should NOT be called under |lock_|.
  void Stop();


  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // Protects |source_|, |audio_tracks_|, |running_|, |loopback_fifo_|,
  // |params_|, |buffering_| and |agc_is_enabled_|.
  mutable base::Lock lock_;

  // A list of audio tracks that the audio data is fed to.
  TrackList tracks_;

  // The audio data source from the browser process.
  scoped_refptr<media::AudioCapturerSource> source_;

  // Buffers used for temporary storage during capture callbacks.
  // Allocated during initialization.
  class ConfiguredBuffer;
  scoped_refptr<ConfiguredBuffer> buffer_;
  bool running_;

  // True when automatic gain control is enabled, false otherwise.
  bool agc_is_enabled_;

  // The media session ID used to identify which input device to be started by
  // the browser.
  int session_id_;

  // The device this capturer is given permission to use.
  std::string device_id_;

  // Stores latest microphone volume received in a CaptureData() callback.
  // Range is [0, 255].
  int volume_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioCapturer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_

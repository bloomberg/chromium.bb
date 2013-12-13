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
#include "base/time/time.h"
#include "content/renderer/media/tagged_list.h"
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
                  int buffer_size,
                  int session_id,
                  const std::string& device_id,
                  int paired_output_sample_rate,
                  int paired_output_frames_per_buffer,
                  int effects);

  // Add a audio track to the sinks of the capturer.
  // WebRtcAudioDeviceImpl calls this method on the main render thread but
  // other clients may call it from other threads. The current implementation
  // does not support multi-thread calling.
  // The first AddTrack will implicitly trigger the Start() of this object.
  // Called on the main render thread or libjingle working thread.
  void AddTrack(WebRtcLocalAudioTrack* track);

  // Remove a audio track from the sinks of the capturer.
  // If the track has been added to the capturer, it  must call RemoveTrack()
  // before it goes away.
  // Called on the main render thread or libjingle working thread.
  void RemoveTrack(WebRtcLocalAudioTrack* track);

  // SetCapturerSource() is called if the client on the source side desires to
  // provide their own captured audio data. Client is responsible for calling
  // Start() on its own source to have the ball rolling.
  // Called on the main render thread.
  void SetCapturerSource(
      const scoped_refptr<media::AudioCapturerSource>& source,
      media::ChannelLayout channel_layout,
      float sample_rate,
      int effects);

  // Called when a stream is connecting to a peer connection. This will set
  // up the native buffer size for the stream in order to optimize the
  // performance for peer connection.
  void EnablePeerConnectionMode();

  // Volume APIs used by WebRtcAudioDeviceImpl.
  // Called on the AudioInputDevice audio thread.
  void SetVolume(int volume);
  int Volume() const;
  int MaxVolume() const;

  bool is_recording() const { return running_; }

  // Audio parameters utilized by the audio capturer. Can be utilized by
  // a local renderer to set up a renderer using identical parameters as the
  // capturer.
  // TODO(phoglund): This accessor is inherently unsafe since the returned
  // parameters can become outdated at any time. Think over the implications
  // of this accessor and if we can remove it.
  media::AudioParameters audio_parameters() const;

  // Gets information about the paired output device. Returns true if such a
  // device exists.
  bool GetPairedOutputParameters(int* session_id,
                                 int* output_sample_rate,
                                 int* output_frames_per_buffer) const;

  const std::string& device_id() const { return device_id_; }
  int session_id() const { return session_id_; }

  // Stops recording audio. This method will empty its track lists since
  // stopping the capturer will implicitly invalidate all its tracks.
  // This method is exposed to the public because the media stream track can
  // call Stop() on its source.
  void Stop();

  // Called by the WebAudioCapturerSource to get the audio processing params.
  // This function is triggered by provideInput() on the WebAudio audio thread,
  // TODO(xians): Remove after moving APM from WebRtc to Chrome.
  void GetAudioProcessingParams(base::TimeDelta* delay, int* volume,
                                bool* key_pressed);

 protected:
  friend class base::RefCountedThreadSafe<WebRtcAudioCapturer>;
  WebRtcAudioCapturer();
  virtual ~WebRtcAudioCapturer();

 private:
  class TrackOwner;
  typedef TaggedList<TrackOwner> TrackList;

  // AudioCapturerSource::CaptureCallback implementation.
  // Called on the AudioInputDevice audio thread.
  virtual void Capture(media::AudioBus* audio_source,
                       int audio_delay_milliseconds,
                       double volume,
                       bool key_pressed) OVERRIDE;
  virtual void OnCaptureError() OVERRIDE;

  // Reconfigures the capturer with a new capture parameters.
  // Must be called without holding the lock.
  void Reconfigure(int sample_rate, media::ChannelLayout channel_layout,
                   int effects);

  // Starts recording audio.
  // Triggered by AddSink() on the main render thread or a Libjingle working
  // thread. It should NOT be called under |lock_|.
  void Start();

  // Helper function to get the buffer size based on |peer_connection_mode_|
  // and sample rate;
  int GetBufferSize(int sample_rate) const;

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // Protects |source_|, |audio_tracks_|, |running_|, |loopback_fifo_|,
  // |params_| and |buffering_|.
  mutable base::Lock lock_;

  // A tagged list of audio tracks that the audio data is fed
  // to. Tagged items need to be notified that the audio format has
  // changed.
  TrackList tracks_;

  // The audio data source from the browser process.
  scoped_refptr<media::AudioCapturerSource> source_;

  // Cached audio parameters for output.
  media::AudioParameters params_;

  bool running_;

  int render_view_id_;

  // Cached value for the hardware native buffer size, used when
  // |peer_connection_mode_| is set to false.
  int hardware_buffer_size_;

  // The media session ID used to identify which input device to be started by
  // the browser.
  int session_id_;

  // The device this capturer is given permission to use.
  std::string device_id_;

  // Stores latest microphone volume received in a CaptureData() callback.
  // Range is [0, 255].
  int volume_;

  // Flag which affects the buffer size used by the capturer.
  bool peer_connection_mode_;

  int output_sample_rate_;
  int output_frames_per_buffer_;

  // Cache value for the audio processing params.
  base::TimeDelta audio_delay_;
  bool key_pressed_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioCapturer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_

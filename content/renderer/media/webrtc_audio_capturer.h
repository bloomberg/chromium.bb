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
#include "content/renderer/media/webrtc_local_audio_renderer.h"
#include "media/audio/audio_input_device.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_fifo.h"

namespace media {
class AudioBus;
}

namespace content {

class WebRtcAudioCapturerSink;
class WebRtcLocalAudioRenderer;

// This class manages the capture data flow by getting data from its
// |source_|, and passing it to its |sink_|.
// It allows clients to inject their own capture data source by calling
// SetCapturerSource(). It is also possible to enable a local sink and
// register a callback which the sink can call when it wants to read captured
// data cached in a FIFO for local loopback rendering.
// The threading model for this class is rather complex since it will be
// created on the main render thread, captured data is provided on a dedicated
// AudioInputDevice thread, and methods can be called either on the Libjingle
// thread or on the main render thread but also other client threads
// if an alternative AudioCapturerSource has been set. In addition, the
// AudioCapturerSource::CaptureEventHandler methods are called on the IO thread
// and requests for data to render is done on the AudioOutputDevice thread.
class CONTENT_EXPORT WebRtcAudioCapturer
    : public base::RefCountedThreadSafe<WebRtcAudioCapturer>,
      NON_EXPORTED_BASE(public media::AudioCapturerSource::CaptureCallback),
      NON_EXPORTED_BASE(public media::AudioCapturerSource::CaptureEventHandler),
      NON_EXPORTED_BASE(
          public content::WebRtcLocalAudioRenderer::LocalRenderCallback) {
 public:
  // Use to construct the audio capturer.
  // Called on the main render thread.
  static scoped_refptr<WebRtcAudioCapturer> CreateCapturer();

  // Creates and configures the default audio capturing source using the
  // provided audio parameters.
  // Called on the main render thread.
  bool Initialize(media::ChannelLayout channel_layout, int sample_rate);

  // Called by the client on the sink side to add a sink.
  // WebRtcAudioDeviceImpl calls this method on the main render thread but
  // other clients may call it from other threads. The current implementation
  // does not support multi-thread calling.
  // TODO(henrika): add lock if we extend number of supported sinks.
  // Called on the main render thread.
  void AddCapturerSink(WebRtcAudioCapturerSink* sink);

  // Called by the client on the sink side to remove a sink.
  // Called on the main render thread.
  // TODO(henrika): add lock if we extend number of supported sinks.
  // Called on the main render thread.
  void RemoveCapturerSink(WebRtcAudioCapturerSink* sink);

  // SetCapturerSource() is called if the client on the source side desires to
  // provide their own captured audio data. Client is responsible for calling
  // Start() on its own source to have the ball rolling.
  // Called on the main render thread.
  void SetCapturerSource(
      const scoped_refptr<media::AudioCapturerSource>& source,
      media::ChannelLayout channel_layout,
      float sample_rate);

  // The |on_device_stopped_cb| callback will be called in OnDeviceStopped().
  // Called on the main render thread.
  void SetStopCallback(const base::Closure& on_device_stopped_cb);

  // Informs this class that a local sink shall be used in addition to the
  // registered WebRtcAudioCapturerSink sink(s). The capturer will enter a
  // buffering mode and store all incoming audio frames in a local FIFO.
  // The renderer will read data from this buffer using the ProvideInput()
  // method.
  // Called on the main render thread.
  void PrepareLoopback();

  // Cancels loopback mode and stops buffering local copies of captured
  // data in the FIFO.
  // Called on the main render thread.
  void CancelLoopback();

  // Pauses buffering of captured data. Does only have an effect if a local
  // sink is used.
  // Called on the main render thread.
  void PauseBuffering();

  // Resumes buffering of captured data. Does only have an effect if a local
  // sink is used.
  // Called on the main render thread.
  void ResumeBuffering();

  // Starts recording audio.
  // Called on the main render thread or a Libjingle working thread.
  void Start();

  // Stops recording audio.
  // Called on the main render thread or a Libjingle working thread.
  void Stop();

  // Sets the microphone volume.
  // Called on the AudioInputDevice audio thread.
  void SetVolume(double volume);

  // Specifies the |session_id| to query which device to use.
  // Called on the main render thread.
  void SetDevice(int session_id);

  // Enables or disables the WebRtc AGC control.
  // Called from a Libjingle working thread.
  void SetAutomaticGainControl(bool enable);

  bool is_recording() const { return running_; }

  // Returns true if a local renderer has called PrepareLoopback() and it can
  // be utilized to prevent more than one local renderer.
  // Called on the main render thread.
  bool IsInLoopbackMode();

  // Audio parameters utilized by the audio capturer. Can be utilized by
  // a local renderer to set up a renderer using identical parameters as the
  // capturer.
  // TODO(phoglund): This accessor is inherently unsafe since the returned
  // parameters can become outdated at any time. Think over the implications
  // of this accessor and if we can remove it.
  media::AudioParameters audio_parameters() const;

  // AudioCapturerSource::CaptureCallback implementation.
  // Called on the AudioInputDevice audio thread.
  virtual void Capture(media::AudioBus* audio_source,
                       int audio_delay_milliseconds,
                       double volume) OVERRIDE;
  virtual void OnCaptureError() OVERRIDE;

  // AudioCapturerSource::CaptureEventHandler implementation.
  // Called on the IO thread.
  virtual void OnDeviceStarted(const std::string& device_id) OVERRIDE;
  virtual void OnDeviceStopped() OVERRIDE;

  // WebRtcLocalAudioRenderer::LocalRenderCallback implementation.
  // Reads stored captured data from a local FIFO. This method is used in
  // combination with a local sink to render captured audio in loopback.
  // This method is called on the AudioOutputDevice worker thread.
  virtual void ProvideInput(media::AudioBus* dest) OVERRIDE;

 protected:
  friend class base::RefCountedThreadSafe<WebRtcAudioCapturer>;
  virtual ~WebRtcAudioCapturer();

 private:
  typedef std::list<WebRtcAudioCapturerSink*> SinkList;

  WebRtcAudioCapturer();

  // Reconfigures the capturer with a new buffer size and capture parameters.
  // Must be called without holding the lock. Returns true on success.
  bool Reconfigure(int sample_rate, media::ChannelLayout channel_layout);

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // Protects |source_|, |sinks_|, |running_|, |on_device_stopped_cb_|,
  // |loopback_fifo_|, |params_|, |buffering_| and |agc_is_enabled_|.
  mutable base::Lock lock_;

  // A list of sinks that the audio data is fed to.
  SinkList sinks_;

  // The audio data source from the browser process.
  scoped_refptr<media::AudioCapturerSource> source_;

  // Buffers used for temporary storage during capture callbacks.
  // Allocated during initialization.
  class ConfiguredBuffer;
  scoped_refptr<ConfiguredBuffer> buffer_;
  std::string device_id_;
  bool running_;

  // Callback object which is called during OnDeviceStopped().
  // Informs a local sink that it should stop asking for data.
  base::Closure on_device_stopped_cb_;

  // Contains copies of captured audio frames. Only utilized in loopback
  // mode when a local sink has been set.
  scoped_ptr<media::AudioFifo> loopback_fifo_;

  // True when FIFO is utilized, false otherwise.
  bool buffering_;

  // True when automatic gain control is enabled, false otherwise.
  bool agc_is_enabled_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioCapturer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_

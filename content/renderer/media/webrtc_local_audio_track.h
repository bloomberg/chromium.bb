// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_

#include <list>
#include <string>

#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/media_stream_audio_track_sink.h"
#include "content/renderer/media/tagged_list.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_source_provider.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreamtrack.h"
#include "third_party/libjingle/source/talk/media/base/audiorenderer.h"

namespace cricket {
class AudioRenderer;
}  // namespace cricket

namespace media {
class AudioBus;
}  // namespace media

namespace content {

class MediaStreamAudioSink;
class MediaStreamAudioSinkOwner;
class PeerConnectionAudioSink;
class WebAudioCapturerSource;
class WebRtcAudioCapturer;

// A WebRtcLocalAudioTrack instance contains the implementations of
// MediaStreamTrack and MediaStreamAudioSink.
// When an instance is created, it will register itself as a track to the
// WebRtcAudioCapturer to get the captured data, and forward the data to
// its |sinks_|. The data flow can be stopped by disabling the audio track.
class CONTENT_EXPORT WebRtcLocalAudioTrack
    : NON_EXPORTED_BASE(public cricket::AudioRenderer),
      NON_EXPORTED_BASE(
          public webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>) {
 public:
  static scoped_refptr<WebRtcLocalAudioTrack> Create(
      const std::string& id,
      const scoped_refptr<WebRtcAudioCapturer>& capturer,
      WebAudioCapturerSource* webaudio_source,
      webrtc::AudioSourceInterface* track_source,
      const webrtc::MediaConstraintsInterface* constraints);

  // Add a sink to the track. This function will trigger a OnSetFormat()
  // call on the |sink|.
  // Called on the main render thread.
  void AddSink(MediaStreamAudioSink* sink);

  // Remove a sink from the track.
  // Called on the main render thread.
  void RemoveSink(MediaStreamAudioSink* sink);

  // Add/remove PeerConnection sink to/from the track.
  // TODO(xians): Remove these two methods after PeerConnection can use the
  // same sink interface as MediaStreamAudioSink.
  void AddSink(PeerConnectionAudioSink* sink);
  void RemoveSink(PeerConnectionAudioSink* sink);

  // Starts the local audio track. Called on the main render thread and
  // should be called only once when audio track is created.
  void Start();

  // Stops the local audio track. Called on the main render thread and
  // should be called only once when audio track going away.
  void Stop();

  // Method called by the capturer to deliver the capture data.
  // Call on the capture audio thread.
  void Capture(media::AudioBus* audio_source,
               int audio_delay_milliseconds,
               int volume,
               bool key_pressed);

  // Method called by the capturer to set the audio parameters used by source
  // of the capture data..
  // Call on the capture audio thread.
  void OnSetFormat(const media::AudioParameters& params);

  blink::WebAudioSourceProvider* audio_source_provider() const {
    return source_provider_.get();
  }

 protected:
  WebRtcLocalAudioTrack(
      const std::string& label,
      const scoped_refptr<WebRtcAudioCapturer>& capturer,
      WebAudioCapturerSource* webaudio_source,
      webrtc::AudioSourceInterface* track_source,
      const webrtc::MediaConstraintsInterface* constraints);

  virtual ~WebRtcLocalAudioTrack();

 private:
  typedef TaggedList<MediaStreamAudioTrackSink> SinkList;

  // cricket::AudioCapturer implementation.
  virtual void AddChannel(int channel_id) OVERRIDE;
  virtual void RemoveChannel(int channel_id) OVERRIDE;

  // webrtc::AudioTrackInterface implementation.
  virtual webrtc::AudioSourceInterface* GetSource() const OVERRIDE;
  virtual cricket::AudioRenderer* GetRenderer() OVERRIDE;

  // webrtc::MediaStreamTrack implementation.
  virtual std::string kind() const OVERRIDE;

  // The provider of captured data to render.
  // The WebRtcAudioCapturer is today created by WebRtcAudioDeviceImpl.
  scoped_refptr<WebRtcAudioCapturer> capturer_;

  // The source of the audio track which is used by WebAudio, which provides
  // data to the audio track when hooking up with WebAudio.
  scoped_refptr<WebAudioCapturerSource> webaudio_source_;

  // The source of the audio track which handles the audio constraints.
  // TODO(xians): merge |track_source_| to |capturer_|.
  talk_base::scoped_refptr<webrtc::AudioSourceInterface> track_source_;

  // A tagged list of sinks that the audio data is fed to. Tags
  // indicate tracks that need to be notified that the audio format
  // has changed.
  SinkList sinks_;

  // Used to DCHECK that some methods are called on the main render thread.
  base::ThreadChecker main_render_thread_checker_;

  // Used to DCHECK that some methods are called on the capture audio thread.
  base::ThreadChecker capture_thread_checker_;

  // Protects |params_| and |sinks_|.
  mutable base::Lock lock_;

  // A vector of WebRtc VoE channels that the capturer sends data to.
  std::vector<int> voe_channels_;

  bool need_audio_processing_;

  // Buffers used for temporary storage during capture callbacks.
  // Allocated and accessed only on the capture audio thread.
  class ConfiguredBuffer;
  scoped_ptr<ConfiguredBuffer> buffer_;

  // The source provider to feed the track data to other clients like
  // WebAudio.
  scoped_ptr<WebRtcLocalAudioSourceProvider> source_provider_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalAudioTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_

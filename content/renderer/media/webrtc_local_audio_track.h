// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_

#include <list>
#include <string>

#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreamtrack.h"
#include "third_party/libjingle/source/talk/media/base/audiorenderer.h"

namespace cricket {
class AudioRenderer;
}

namespace content {

class WebRtcAudioCapturer;
class WebRtcAudioCapturerSinkOwner;

// A WebRtcLocalAudioTrack instance contains the implementations of
// MediaStreamTrack and WebRtcAudioCapturerSink.
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
      webrtc::AudioSourceInterface* stream_source);

  // Add a sink to the track. This function will trigger a SetCaptureFormat()
  // call on the |sink|.
  // Called on the main render thread.
  void AddSink(WebRtcAudioCapturerSink* sink);

  // Remove a sink from the track.
  // Called on the main render thread.
  void RemoveSink(WebRtcAudioCapturerSink* sink);

  // Starts the local audio track. Called on the main render thread and
  // should be called only once when audio track is created.
  void Start();

  // Stops the local audio track. Called on the main render thread and
  // should be called only once when audio track going away.
  void Stop();

  // Method called by the capturer to deliever the capture data.
  void CaptureData(const int16* audio_data,
                   int number_of_channels,
                   int number_of_frames,
                   int audio_delay_milliseconds,
                   int volume);

  // Method called by the capturer to set the audio parameters used by source
  // of the capture data..
  // Can be called on different user threads.
  void SetCaptureFormat(const media::AudioParameters& params);

 protected:
  WebRtcLocalAudioTrack(const std::string& label,
                        const scoped_refptr<WebRtcAudioCapturer>& capturer,
                        webrtc::AudioSourceInterface* track_source);
  virtual ~WebRtcLocalAudioTrack();

 private:
  typedef std::list<scoped_refptr<WebRtcAudioCapturerSinkOwner> > SinkList;

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

  // The source of the audio track which handles the audio constraints.
  // TODO(xians): merge |track_source_| to |capturer_|.
  talk_base::scoped_refptr<webrtc::AudioSourceInterface> track_source_;

  // A list of sinks that the audio data is fed to.
  SinkList sinks_;

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // Cached values of the audio parameters used by the |source_| and |sinks_|.
  media::AudioParameters params_;

  // Protects |params_| and |sinks_|.
  mutable base::Lock lock_;

  // A vector of WebRtc VoE channels that the capturer sends datat to.
  std::vector<int> voe_channels_;

  bool need_audio_processing_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalAudioTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_

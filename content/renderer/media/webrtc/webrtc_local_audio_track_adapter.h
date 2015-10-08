// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_LOCAL_AUDIO_TRACK_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_LOCAL_AUDIO_TRACK_ADAPTER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreamtrack.h"
#include "third_party/libjingle/source/talk/media/base/audiorenderer.h"

namespace cricket {
class AudioRenderer;
}

namespace webrtc {
class AudioSourceInterface;
class AudioProcessorInterface;
}

namespace content {

class MediaStreamAudioProcessor;
class WebRtcAudioSinkAdapter;
class WebRtcLocalAudioTrack;

class CONTENT_EXPORT WebRtcLocalAudioTrackAdapter
    : NON_EXPORTED_BASE(
          public webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>) {
 public:
  static scoped_refptr<WebRtcLocalAudioTrackAdapter> Create(
      const std::string& label,
      webrtc::AudioSourceInterface* track_source);

  WebRtcLocalAudioTrackAdapter(
      const std::string& label,
      webrtc::AudioSourceInterface* track_source,
      const scoped_refptr<base::SingleThreadTaskRunner>& signaling_thread);

  ~WebRtcLocalAudioTrackAdapter() override;

  void Initialize(WebRtcLocalAudioTrack* owner);

  // Called on the audio thread by the WebRtcLocalAudioTrack to set the signal
  // level of the audio data.
  void SetSignalLevel(int signal_level);

  // Method called by the WebRtcLocalAudioTrack to set the processor that
  // applies signal processing on the data of the track.
  // This class will keep a reference of the |processor|.
  // Called on the main render thread.
  void SetAudioProcessor(
      const scoped_refptr<MediaStreamAudioProcessor>& processor);

  // webrtc::MediaStreamTrack implementation.
  std::string kind() const override;
  bool set_enabled(bool enable) override;

 private:
  // webrtc::AudioTrackInterface implementation.
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override;
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override;
  bool GetSignalLevel(int* level) override;
  rtc::scoped_refptr<webrtc::AudioProcessorInterface> GetAudioProcessor()
      override;
  webrtc::AudioSourceInterface* GetSource() const override;
  cricket::AudioRenderer* GetRenderer() override;

  // Weak reference.
  WebRtcLocalAudioTrack* owner_;

  // The source of the audio track which handles the audio constraints.
  // TODO(xians): merge |track_source_| to |capturer_| in WebRtcLocalAudioTrack.
  rtc::scoped_refptr<webrtc::AudioSourceInterface> track_source_;

  // Libjingle's signaling thread.
  const scoped_refptr<base::SingleThreadTaskRunner> signaling_thread_;

  // The audio processsor that applies audio processing on the data of audio
  // track.
  scoped_refptr<MediaStreamAudioProcessor> audio_processor_;

  // A vector of WebRtc VoE channels that the capturer sends data to.
  std::vector<int> voe_channels_;

  // A vector of the peer connection sink adapters which receive the audio data
  // from the audio track.
  ScopedVector<WebRtcAudioSinkAdapter> sink_adapters_;

  // The amplitude of the signal.
  int signal_level_;

  // Thread checker for libjingle's signaling thread.
  base::ThreadChecker signaling_thread_checker_;
  base::ThreadChecker capture_thread_;

  // Protects |voe_channels_|, |audio_processor_| and |signal_level_|.
  mutable base::Lock lock_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_LOCAL_AUDIO_TRACK_ADAPTER_H_

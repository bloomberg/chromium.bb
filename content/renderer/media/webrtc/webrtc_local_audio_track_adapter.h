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
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_audio_level_calculator.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "third_party/webrtc/api/mediastreamtrack.h"
#include "third_party/webrtc/media/base/audiorenderer.h"

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

// Provides an implementation of the webrtc::AudioTrackInterface that can be
// bound/unbound to/from a MediaStreamAudioTrack.  In other words, this is an
// adapter that sits between the media stream object graph and WebRtc's object
// graph and proxies between the two.
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
      scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner);

  ~WebRtcLocalAudioTrackAdapter() override;

  void Initialize(WebRtcLocalAudioTrack* owner);

  // Set the object that provides shared access to the current audio signal
  // level.  This method may only be called once, before the audio data flow
  // starts, and before any calls to GetSignalLevel() might be made.
  void SetLevel(scoped_refptr<MediaStreamAudioLevelCalculator::Level> level);

  // Method called by the WebRtcLocalAudioTrack to set the processor that
  // applies signal processing on the data of the track.
  // This class will keep a reference of the |processor|.
  // Called on the main render thread.
  // This method may only be called once, before the audio data flow starts, and
  // before any calls to GetAudioProcessor() might be made.
  void SetAudioProcessor(scoped_refptr<MediaStreamAudioProcessor> processor);

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

  // Weak reference.
  WebRtcLocalAudioTrack* owner_;

  // The source of the audio track which handles the audio constraints.
  // TODO(xians): merge |track_source_| to |capturer_| in WebRtcLocalAudioTrack.
  rtc::scoped_refptr<webrtc::AudioSourceInterface> track_source_;

  // Libjingle's signaling thread.
  const scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner_;

  // The audio processsor that applies audio processing on the data of audio
  // track.  This must be set before calls to GetAudioProcessor() are made.
  scoped_refptr<MediaStreamAudioProcessor> audio_processor_;

  // A vector of the peer connection sink adapters which receive the audio data
  // from the audio track.
  ScopedVector<WebRtcAudioSinkAdapter> sink_adapters_;

  // Thread-safe accessor to current audio signal level.  This must be set
  // before calls to GetSignalLevel() are made.
  scoped_refptr<MediaStreamAudioLevelCalculator::Level> level_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_LOCAL_AUDIO_TRACK_ADAPTER_H_

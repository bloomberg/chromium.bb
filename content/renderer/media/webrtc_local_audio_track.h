// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_

#include <list>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/tagged_list.h"
#include "media/audio/audio_parameters.h"

namespace media {
class AudioBus;
}

namespace content {

class MediaStreamAudioLevelCalculator;
class MediaStreamAudioProcessor;
class MediaStreamAudioSink;
class MediaStreamAudioSinkOwner;
class MediaStreamAudioTrackSink;
class WebAudioCapturerSource;
class WebRtcAudioCapturer;
class WebRtcLocalAudioTrackAdapter;

// A WebRtcLocalAudioTrack instance contains the implementations of
// MediaStreamTrackExtraData.
// When an instance is created, it will register itself as a track to the
// WebRtcAudioCapturer to get the captured data, and forward the data to
// its |sinks_|. The data flow can be stopped by disabling the audio track.
// TODO(tommi): Rename to MediaStreamLocalAudioTrack.
class CONTENT_EXPORT WebRtcLocalAudioTrack
    : NON_EXPORTED_BASE(public MediaStreamAudioTrack) {
 public:
  WebRtcLocalAudioTrack(WebRtcLocalAudioTrackAdapter* adapter,
                        const scoped_refptr<WebRtcAudioCapturer>& capturer,
                        WebAudioCapturerSource* webaudio_source);

  ~WebRtcLocalAudioTrack() override;

  // Add a sink to the track. This function will trigger a OnSetFormat()
  // call on the |sink|.
  // Called on the main render thread.
  void AddSink(MediaStreamAudioSink* sink) override;

  // Remove a sink from the track.
  // Called on the main render thread.
  void RemoveSink(MediaStreamAudioSink* sink) override;

  // Starts the local audio track. Called on the main render thread and
  // should be called only once when audio track is created.
  void Start();

  // Overrides for MediaStreamTrack.

  void SetEnabled(bool enabled) override;

  // Stops the local audio track. Called on the main render thread and
  // should be called only once when audio track going away.
  void Stop() override;

  webrtc::AudioTrackInterface* GetAudioAdapter() override;

  // Returns the output format of the capture source. May return an invalid
  // AudioParameters if the format is not yet available.
  // Called on the main render thread.
  media::AudioParameters GetOutputFormat() const override;

  // Method called by the capturer to deliver the capture data.
  // Called on the capture audio thread.
  void Capture(const media::AudioBus& audio_bus,
               base::TimeTicks estimated_capture_time,
               bool force_report_nonzero_energy);

  // Method called by the capturer to set the audio parameters used by source
  // of the capture data..
  // Called on the capture audio thread.
  void OnSetFormat(const media::AudioParameters& params);

  // Method called by the capturer to set the processor that applies signal
  // processing on the data of the track.
  // Called on the capture audio thread.
  void SetAudioProcessor(
      const scoped_refptr<MediaStreamAudioProcessor>& processor);

 private:
  typedef TaggedList<MediaStreamAudioTrackSink> SinkList;

  // All usage of libjingle is through this adapter. The adapter holds
  // a pointer to this object, but no reference.
  const scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter_;

  // The provider of captured data to render.
  scoped_refptr<WebRtcAudioCapturer> capturer_;

  // The source of the audio track which is used by WebAudio, which provides
  // data to the audio track when hooking up with WebAudio.
  scoped_refptr<WebAudioCapturerSource> webaudio_source_;

  // A tagged list of sinks that the audio data is fed to. Tags
  // indicate tracks that need to be notified that the audio format
  // has changed.
  SinkList sinks_;

  // Tests that methods are called on libjingle's signaling thread.
  base::ThreadChecker signal_thread_checker_;

  // Used to DCHECK that some methods are called on the capture audio thread.
  base::ThreadChecker capture_thread_checker_;

  // Protects |params_| and |sinks_|.
  mutable base::Lock lock_;

  // Audio parameters of the audio capture stream.
  // Accessed on only the audio capture thread.
  media::AudioParameters audio_parameters_;

  // Used to calculate the signal level that shows in the UI.
  // Accessed on only the audio thread.
  scoped_ptr<MediaStreamAudioLevelCalculator> level_calculator_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalAudioTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_

#include <list>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/tagged_list.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "media/base/audio_parameters.h"

namespace media {
class AudioBus;
}

namespace content {

class MediaStreamAudioLevelCalculator;
class MediaStreamAudioProcessor;
class MediaStreamAudioSink;
class MediaStreamAudioSinkOwner;
class MediaStreamAudioTrackSink;

// A WebRtcLocalAudioTrack manages thread-safe connects/disconnects to sinks,
// and the delivery of audio data from the source to the sinks.
class CONTENT_EXPORT WebRtcLocalAudioTrack
    : NON_EXPORTED_BASE(public MediaStreamAudioTrack) {
 public:
  explicit WebRtcLocalAudioTrack(
      scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter);

  ~WebRtcLocalAudioTrack() override;

  // Add a sink to the track. This function will trigger a OnSetFormat()
  // call on the |sink|.
  // Called on the main render thread.
  void AddSink(MediaStreamAudioSink* sink) override;

  // Remove a sink from the track.
  // Called on the main render thread.
  void RemoveSink(MediaStreamAudioSink* sink) override;

  // Overrides for MediaStreamTrack.
  void SetEnabled(bool enabled) override;
  webrtc::AudioTrackInterface* GetAudioAdapter() override;
  media::AudioParameters GetOutputFormat() const override;

  // Method called by the capturer to deliver the capture data.
  // Called on the capture audio thread.
  void Capture(const media::AudioBus& audio_bus,
               base::TimeTicks estimated_capture_time);

  // Method called by the capturer to set the audio parameters used by source
  // of the capture data..
  // Called on the capture audio thread.
  void OnSetFormat(const media::AudioParameters& params);

  // Called by the capturer before the audio data flow begins to set the object
  // that provides shared access to the current audio signal level.
  void SetLevel(scoped_refptr<MediaStreamAudioLevelCalculator::Level> level);

  // Called by the capturer before the audio data flow begins to provide a
  // reference to the audio processor so that the track can query stats from it.
  void SetAudioProcessor(scoped_refptr<MediaStreamAudioProcessor> processor);

 private:
  typedef TaggedList<MediaStreamAudioTrackSink> SinkList;

  // MediaStreamAudioTrack override.
  void OnStop() final;

  // All usage of libjingle is through this adapter. The adapter holds
  // a pointer to this object, but no reference.
  const scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter_;

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
  media::AudioParameters audio_parameters_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalAudioTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_

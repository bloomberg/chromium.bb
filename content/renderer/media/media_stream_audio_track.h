// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/renderer/media/media_stream_track.h"

namespace webrtc {
class AudioTrackInterface;
}  // namespace webrtc

namespace content {

class MediaStreamAudioSink;

// TODO(miu): In a soon-upcoming set of refactoring changes, this class will
// take on the functionality of managing sink connections and the audio data
// flow between source and sinks.  The WebRTC-specific elements will then be
// moved into a subclass.   http://crbug.com/577874
class CONTENT_EXPORT MediaStreamAudioTrack : public MediaStreamTrack {
 public:
  explicit MediaStreamAudioTrack(bool is_local_track);

  // Subclasses must ensure the track is stopped (i.e., Stop() has been called
  // at least once) before this destructor is invoked.
  ~MediaStreamAudioTrack() override;

  // Returns the MediaStreamAudioTrack instance owned by the given blink |track|
  // or null.
  static MediaStreamAudioTrack* From(const blink::WebMediaStreamTrack& track);

  // Add a sink to the track. This function will trigger a OnSetFormat()
  // call on the |sink|.
  // Called on the main render thread.
  virtual void AddSink(MediaStreamAudioSink* sink) = 0;

  // Remove a sink from the track.
  // Called on the main render thread.
  virtual void RemoveSink(MediaStreamAudioSink* sink) = 0;

  // TODO(tommi, xians): Remove this method.
  // TODO(miu): See class-level TODO comment.  ;-)
  virtual webrtc::AudioTrackInterface* GetAudioAdapter();

  // Returns the output format of the capture source. May return an invalid
  // AudioParameters if the format is not yet available.
  // Called on the main render thread.
  // TODO(tommi): This method appears to only be used by Pepper and in fact
  // does not appear to be necessary there.  We should remove it since it adds
  // to the complexity of all types of audio tracks+source implementations.
  virtual media::AudioParameters GetOutputFormat() const = 0;

  // Called to notify this track that the flow of audio data has started from
  // the source.  |stop_callback| is run by Stop() when the source must halt the
  // flow of audio data to this track.
  void Start(const base::Closure& stop_callback);

  // Halts the flow of audio data from the source (and to the sinks), and then
  // invokes OnStop() to perform any additional actions.
  void Stop() final;

 protected:
  // Called by Stop() after the source has halted the flow of audio data.
  virtual void OnStop();

 private:
  // Callback provided to Start() which is run when the audio flow must halt.
  base::Closure stop_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_H_

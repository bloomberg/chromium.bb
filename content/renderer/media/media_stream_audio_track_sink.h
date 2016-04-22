// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_SINK_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_SINK_H_

#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"

namespace media {
class AudioBus;
}

namespace content {

class MediaStreamAudioSink;

// Interface for reference counted holder of audio stream audio track sink.
class MediaStreamAudioTrackSink
    : public base::RefCountedThreadSafe<MediaStreamAudioTrackSink> {
 public:
  // Note: OnData() and OnSetFormat() have the same meaning and semantics as in
  // content::MediaStreamAudioSink.  See comments there for usage info.
  virtual void OnData(const media::AudioBus& audio_bus,
                      base::TimeTicks estimated_capture_time) = 0;
  virtual void OnSetFormat(const media::AudioParameters& params) = 0;

  virtual void OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) = 0;

  virtual void Reset() = 0;

  virtual bool IsEqual(const MediaStreamAudioSink* other) const = 0;

  // Wrapper which allows to use std::find_if() when adding and removing
  // sinks to/from the list.
  struct WrapsMediaStreamSink {
    WrapsMediaStreamSink(MediaStreamAudioSink* sink) : sink_(sink) {}
    bool operator()(
        const scoped_refptr<MediaStreamAudioTrackSink>& owner) const {
      return owner->IsEqual(sink_);
    }
    MediaStreamAudioSink* sink_;
  };

 protected:
  virtual ~MediaStreamAudioTrackSink() {}

 private:
  friend class base::RefCountedThreadSafe<MediaStreamAudioTrackSink>;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_SINK_H_

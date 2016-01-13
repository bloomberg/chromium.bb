// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_remote_audio_track.h"

#include <stddef.h>

#include <list>

#include "base/logging.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

class MediaStreamRemoteAudioSource::AudioSink
    : public webrtc::AudioTrackSinkInterface {
 public:
  AudioSink() {
  }
  ~AudioSink() override {
    DCHECK(sinks_.empty());
  }

  void Add(MediaStreamAudioSink* sink, MediaStreamAudioTrack* track,
           bool enabled) {
    DCHECK(thread_checker_.CalledOnValidThread());
    SinkInfo info(sink, track, enabled);
    base::AutoLock lock(lock_);
    sinks_.push_back(info);
  }

  void Remove(MediaStreamAudioSink* sink, MediaStreamAudioTrack* track) {
    DCHECK(thread_checker_.CalledOnValidThread());
    base::AutoLock lock(lock_);
    sinks_.remove_if([&sink, &track](const SinkInfo& info) {
        return info.sink == sink && info.track == track;
      });
  }

  void SetEnabled(MediaStreamAudioTrack* track, bool enabled) {
    DCHECK(thread_checker_.CalledOnValidThread());
    base::AutoLock lock(lock_);
    for (SinkInfo& info : sinks_) {
      if (info.track == track)
        info.enabled = enabled;
    }
  }

  void RemoveAll(MediaStreamAudioTrack* track) {
    base::AutoLock lock(lock_);
    sinks_.remove_if([&track](const SinkInfo& info) {
        return info.track == track;
      });
  }

  bool IsNeeded() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return !sinks_.empty();
  }

 private:
  void OnData(const void* audio_data, int bits_per_sample, int sample_rate,
              size_t number_of_channels, size_t number_of_frames) override {
    if (!audio_bus_ ||
        static_cast<size_t>(audio_bus_->channels()) != number_of_channels ||
        static_cast<size_t>(audio_bus_->frames()) != number_of_frames) {
      audio_bus_ = media::AudioBus::Create(number_of_channels,
                                           number_of_frames);
    }

    audio_bus_->FromInterleaved(audio_data, number_of_frames,
                                bits_per_sample / 8);

    bool format_changed = false;
    if (params_.format() != media::AudioParameters::AUDIO_PCM_LOW_LATENCY ||
        static_cast<size_t>(params_.channels()) != number_of_channels ||
        params_.sample_rate() != sample_rate ||
        static_cast<size_t>(params_.frames_per_buffer()) != number_of_frames) {
      params_ = media::AudioParameters(
          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
          media::GuessChannelLayout(number_of_channels),
          sample_rate, 16, number_of_frames);
      format_changed = true;
    }

    // TODO(tommi): We should get the timestamp from WebRTC.
    base::TimeTicks estimated_capture_time(base::TimeTicks::Now());

    base::AutoLock lock(lock_);
    for (const SinkInfo& info : sinks_) {
      if (info.enabled) {
        if (format_changed)
          info.sink->OnSetFormat(params_);
        info.sink->OnData(*audio_bus_.get(), estimated_capture_time);
      }
    }
  }

  mutable base::Lock lock_;
  struct SinkInfo {
    SinkInfo(MediaStreamAudioSink* sink, MediaStreamAudioTrack* track,
             bool enabled) : sink(sink), track(track), enabled(enabled) {}
    MediaStreamAudioSink* sink;
    MediaStreamAudioTrack* track;
    bool enabled;
  };
  std::list<SinkInfo> sinks_;
  base::ThreadChecker thread_checker_;
  media::AudioParameters params_;  // Only used on the callback thread.
  scoped_ptr<media::AudioBus> audio_bus_;  // Only used on the callback thread.
};

MediaStreamRemoteAudioTrack::MediaStreamRemoteAudioTrack(
    const blink::WebMediaStreamSource& source, bool enabled)
    : MediaStreamAudioTrack(false), source_(source), enabled_(enabled) {
  DCHECK(source.extraData());  // Make sure the source has a native source.
}

MediaStreamRemoteAudioTrack::~MediaStreamRemoteAudioTrack() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  source()->RemoveAll(this);
}

void MediaStreamRemoteAudioTrack::SetEnabled(bool enabled) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  // This affects the shared state of the source for whether or not it's a part
  // of the mixed audio that's rendered for remote tracks from WebRTC.
  // All tracks from the same source will share this state and thus can step
  // on each other's toes.
  // This is also why we can't check the |enabled_| state for equality with
  // |enabled| before setting the mixing enabled state. |enabled_| and the
  // shared state might not be the same.
  source()->SetEnabledForMixing(enabled);

  enabled_ = enabled;
  source()->SetSinksEnabled(this, enabled);
}

void MediaStreamRemoteAudioTrack::Stop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Stop means that a track should be stopped permanently. But
  // since there is no proper way of doing that on a remote track, we can
  // at least disable the track. Blink will not call down to the content layer
  // after a track has been stopped.
  SetEnabled(false);
}

void MediaStreamRemoteAudioTrack::AddSink(MediaStreamAudioSink* sink) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  return source()->AddSink(sink, this, enabled_);
}

void MediaStreamRemoteAudioTrack::RemoveSink(MediaStreamAudioSink* sink) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  return source()->RemoveSink(sink, this);
}

media::AudioParameters MediaStreamRemoteAudioTrack::GetOutputFormat() const {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // This method is not implemented on purpose and should be removed.
  // TODO(tommi): See comment for GetOutputFormat in MediaStreamAudioTrack.
  NOTIMPLEMENTED();
  return media::AudioParameters();
}

webrtc::AudioTrackInterface* MediaStreamRemoteAudioTrack::GetAudioAdapter() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  return source()->GetAudioAdapter();
}

MediaStreamRemoteAudioSource* MediaStreamRemoteAudioTrack::source() const {
  return static_cast<MediaStreamRemoteAudioSource*>(source_.extraData());
}

MediaStreamRemoteAudioSource::MediaStreamRemoteAudioSource(
    const scoped_refptr<webrtc::AudioTrackInterface>& track) : track_(track) {}

MediaStreamRemoteAudioSource::~MediaStreamRemoteAudioSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MediaStreamRemoteAudioSource::SetEnabledForMixing(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  track_->set_enabled(enabled);
}

void MediaStreamRemoteAudioSource::AddSink(MediaStreamAudioSink* sink,
                                           MediaStreamAudioTrack* track,
                                           bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!sink_) {
    sink_.reset(new AudioSink());
    track_->AddSink(sink_.get());
  }

  sink_->Add(sink, track, enabled);
}

void MediaStreamRemoteAudioSource::RemoveSink(MediaStreamAudioSink* sink,
                                              MediaStreamAudioTrack* track) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sink_);

  sink_->Remove(sink, track);

  if (!sink_->IsNeeded()) {
    track_->RemoveSink(sink_.get());
    sink_.reset();
  }
}

void MediaStreamRemoteAudioSource::SetSinksEnabled(MediaStreamAudioTrack* track,
                                                   bool enabled) {
  if (sink_)
    sink_->SetEnabled(track, enabled);
}

void MediaStreamRemoteAudioSource::RemoveAll(MediaStreamAudioTrack* track) {
  if (sink_)
    sink_->RemoveAll(track);
}

webrtc::AudioTrackInterface* MediaStreamRemoteAudioSource::GetAudioAdapter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return track_.get();
}

}  // namespace content

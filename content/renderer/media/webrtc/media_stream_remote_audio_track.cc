// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_remote_audio_track.h"

#include <list>

#include "base/logging.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

class MediaStreamRemoteAudioTrack::AudioSink
    : public webrtc::AudioTrackSinkInterface {
 public:
  AudioSink() {
  }
  ~AudioSink() override {
    DCHECK(sinks_.empty());
  }

  void Add(MediaStreamAudioSink* sink) {
    DCHECK(thread_checker_.CalledOnValidThread());
    base::AutoLock lock(lock_);
    sinks_.push_back(sink);
  }

  void Remove(MediaStreamAudioSink* sink) {
    DCHECK(thread_checker_.CalledOnValidThread());
    base::AutoLock lock(lock_);
    sinks_.remove(sink);
  }

  bool IsNeeded() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return !sinks_.empty();
  }

  const media::AudioParameters GetOutputFormat() const {
    base::AutoLock lock(lock_);
    return params_;
  }

 private:
  void OnData(const void* audio_data, int bits_per_sample, int sample_rate,
              int number_of_channels, size_t number_of_frames) override {
    if (!audio_bus_ || audio_bus_->channels() != number_of_channels ||
        audio_bus_->frames() != static_cast<int>(number_of_frames)) {
      audio_bus_ = media::AudioBus::Create(number_of_channels,
                                           number_of_frames);
    }

    audio_bus_->FromInterleaved(audio_data, number_of_frames,
                                bits_per_sample / 8);

    bool format_changed = false;
    if (params_.format() != media::AudioParameters::AUDIO_PCM_LOW_LATENCY ||
        params_.channels() != number_of_channels ||
        params_.sample_rate() != sample_rate ||
        params_.frames_per_buffer() != static_cast<int>(number_of_frames)) {
      base::AutoLock lock(lock_);  // Only lock on this thread when writing.
      params_ = media::AudioParameters(
          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
          media::GuessChannelLayout(number_of_channels),
          sample_rate, 16, number_of_frames);
      format_changed = true;
    }

    // TODO(tommi): We should get the timestamp from WebRTC.
    base::TimeTicks estimated_capture_time(base::TimeTicks::Now());

    base::AutoLock lock(lock_);
    for (auto* sink : sinks_) {
      if (format_changed)
        sink->OnSetFormat(params_);
      sink->OnData(*audio_bus_.get(), estimated_capture_time);
    }
  }

  mutable base::Lock lock_;
  std::list<MediaStreamAudioSink*> sinks_;
  base::ThreadChecker thread_checker_;
  media::AudioParameters params_;
  scoped_ptr<media::AudioBus> audio_bus_;  // Only used on the callback thread.
};

MediaStreamRemoteAudioTrack::MediaStreamRemoteAudioTrack(
    const scoped_refptr<webrtc::AudioTrackInterface>& track)
    : MediaStreamAudioTrack(false), track_(track) {
}

MediaStreamRemoteAudioTrack::~MediaStreamRemoteAudioTrack() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
}

void MediaStreamRemoteAudioTrack::SetEnabled(bool enabled) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  track_->set_enabled(enabled);
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

  if (!sink_) {
    sink_.reset(new AudioSink());
    track_->AddSink(sink_.get());
  }

  sink_->Add(sink);
}

void MediaStreamRemoteAudioTrack::RemoveSink(MediaStreamAudioSink* sink) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  DCHECK(sink_);

  sink_->Remove(sink);

  if (!sink_->IsNeeded()) {
    track_->RemoveSink(sink_.get());
    sink_.reset();
  }
}

media::AudioParameters MediaStreamRemoteAudioTrack::GetOutputFormat() const {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  return sink_ ? sink_->GetOutputFormat() : media::AudioParameters();
}

webrtc::AudioTrackInterface* MediaStreamRemoteAudioTrack::GetAudioAdapter() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  return track_.get();
}

}  // namespace content

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_sink_owner.h"

#include "content/public/renderer/media_stream_audio_sink.h"
#include "media/audio/audio_parameters.h"

namespace content {

MediaStreamAudioSinkOwner::MediaStreamAudioSinkOwner(MediaStreamAudioSink* sink)
    : delegate_(sink) {
}

void MediaStreamAudioSinkOwner::OnData(const media::AudioBus& audio_bus,
                                       base::TimeTicks estimated_capture_time) {
  base::AutoLock lock(lock_);
  if (delegate_)
    delegate_->OnData(audio_bus, estimated_capture_time);
}

void MediaStreamAudioSinkOwner::OnSetFormat(
    const media::AudioParameters& params) {
  base::AutoLock lock(lock_);
  if (delegate_)
    delegate_->OnSetFormat(params);
}

void MediaStreamAudioSinkOwner::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  base::AutoLock lock(lock_);
  if (delegate_)
    delegate_->OnReadyStateChanged(state);
}

void MediaStreamAudioSinkOwner::Reset() {
  base::AutoLock lock(lock_);
  delegate_ = NULL;
}

bool MediaStreamAudioSinkOwner::IsEqual(
    const MediaStreamAudioSink* other) const {
  DCHECK(other);
  base::AutoLock lock(lock_);
  return (other == delegate_);
}

}  // namespace content

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filters.h"

#include "base/logging.h"

namespace media {

// static
const size_t DataSource::kReadError = static_cast<size_t>(-1);

void ResetAndRunCB(FilterStatusCB* cb, PipelineStatus status) {
  DCHECK(!cb->is_null());
  FilterStatusCB tmp_cb(*cb);
  cb->Reset();
  tmp_cb.Run(status);
}

void ResetAndRunCB(base::Closure* cb) {
  DCHECK(!cb->is_null());
  base::Closure tmp_cb(*cb);
  cb->Reset();
  tmp_cb.Run();
}

Filter::Filter() : host_(NULL) {}

Filter::~Filter() {}

void Filter::set_host(FilterHost* host) {
  DCHECK(host);
  DCHECK(!host_);
  host_ = host;
}

FilterHost* Filter::host() {
  return host_;
}

void Filter::Play(const base::Closure& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

void Filter::Pause(const base::Closure& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

void Filter::Flush(const base::Closure& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

void Filter::Stop(const base::Closure& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

void Filter::SetPlaybackRate(float playback_rate) {}

void Filter::Seek(base::TimeDelta time, const FilterStatusCB& callback) {
  DCHECK(!callback.is_null());
  callback.Run(PIPELINE_OK);
}

void Filter::OnAudioRendererDisabled() {
}

VideoDecoder::VideoDecoder() {}

VideoDecoder::~VideoDecoder() {}

AudioDecoder::AudioDecoder() {}

AudioDecoder::~AudioDecoder() {}

void AudioDecoder::ConsumeAudioSamples(scoped_refptr<Buffer> buffer) {
  consume_audio_samples_callback_.Run(buffer);
}

}  // namespace media
